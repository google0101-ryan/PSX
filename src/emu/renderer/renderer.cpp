#include <emu/gpu/gpu.h>
#include "renderer.h"
#include "shader.h"

#include <glbinding/gl/gl.h>
#include <glbinding-aux/types_to_string.h>
#include <glbinding/Binding.h>
#include <glbinding/glbinding.h>
#include <SDL2/SDL.h>


namespace renderer
{

const gl::GLuint ATTRIB_INDEX_POSITION = 0;
const gl::GLuint ATTRIB_INDEX_TEXCOORD = 1;

Renderer::Renderer()
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	window = SDL_CreateWindow("PSX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, VRAM_WIDTH*1.5f, VRAM_HEIGHT*1.5f, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	context = SDL_GL_CreateContext(window);

	const glbinding::GetProcAddress get_proc_address = [](const char* name) {
		return reinterpret_cast<glbinding::ProcAddress>(SDL_GL_GetProcAddress(name));
	};
	glbinding::initialize(get_proc_address, false);

	shader_program = load_shaders("screen");

	gl::glUseProgram(shader_program);

	gl::glGenVertexArrays(1, &vao);
	gl::glBindVertexArray(vao);

	gl::glGenBuffers(1, &vbo);
	gl::glBindBuffer(gl::GL_ARRAY_BUFFER, vbo);

	const auto x = 1.f;
	const auto y = 1.f;

	const float vertices[] = 
	{
		// Position Texcoords
		-1.f, 1.f, 0.0f, 0.0f,  // Top-left
		-1.f, -y,  0.0f, 1.0f,  // Bottom-left
		x,    1.f, 1.0f, 0.0f,  // Top-right
		x,    -y,  1.0f, 1.0f,  // Bottom-right
	};

	gl::glBufferData(gl::GL_ARRAY_BUFFER, sizeof(vertices), vertices, gl::GL_STATIC_DRAW);

	const auto vertex_stride = 4 * sizeof(float);
	const auto position_offset = 0 * sizeof(float);
	const auto texcoord_offset = 2 * sizeof(float);

	gl::glEnableVertexAttribArray(ATTRIB_INDEX_POSITION);
	gl::glVertexAttribPointer(ATTRIB_INDEX_POSITION, 2, gl::GL_FLOAT, gl::GL_FALSE, vertex_stride,
							(const void*)position_offset);

	gl::glEnableVertexAttribArray(ATTRIB_INDEX_TEXCOORD);
	gl::glVertexAttribPointer(ATTRIB_INDEX_TEXCOORD, 2, gl::GL_FLOAT, gl::GL_FALSE, vertex_stride,
							(const void*)texcoord_offset);

	// Generate and configure screen texture
	gl::glGenTextures(1, &tex_screen);
	bind_screen_texture();

	gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
	gl::glTexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);

	// Get uniforms
	tex_size = gl::glGetUniformLocation(shader_program, "u_tex_size");

	gl::glBindVertexArray(0);
}

void Renderer::render(const void *vram_data) const
{
	SDL_GL_MakeCurrent(window, context);

	gl::glBindVertexArray(vao);
	gl::glUseProgram(shader_program);
	bind_screen_texture();

	gl::glPixelStorei(gl::GL_UNPACK_ROW_LENGTH, 1024);
	gl::glTexSubImage2D(gl::GL_TEXTURE_2D, 0, 0, 0, screen_width, screen_height, gl::GL_RGBA, gl::GL_UNSIGNED_SHORT_1_5_5_5_REV, vram_data);
	gl::glPixelStorei(gl::GL_UNPACK_ROW_LENGTH, 0);

	gl::glUniform2f(tex_size, (float)screen_width, (float)screen_height);
	gl::glDrawArrays(gl::GL_TRIANGLE_STRIP, 0, 4);

	SDL_GL_SwapWindow(window);
}

void Renderer::bind_screen_texture() const
{
	gl::glActiveTexture(gl::GL_TEXTURE0);
	gl::glBindTexture(gl::GL_TEXTURE_2D, tex_screen);
}

void Renderer::set_texture_size(int32_t width, int32_t height)
{
	bind_screen_texture();

	if (width != screen_width || height != screen_height)
	{
		gl::glTexImage2D(gl::GL_TEXTURE_2D, 0, gl::GL_RGBA, width, height, 0, gl::GL_RGBA, gl::GL_UNSIGNED_SHORT, nullptr);
		screen_width = width;
		screen_height = height;
	}
}

template <PixelRenderType RenderType>
void Renderer::draw_pixel(Position pos, const Color3 *col, const TextureInfo *tex_info, BarycentricCoords bar, int32_t area, DrawCommand::Flags draw_flags)
{
	TexelPos texel {};
	constexpr bool is_textured = RenderType != PixelRenderType::SHADED;

	if (is_textured)
		texel = calculate_texel_pos(bar, area, tex_info->uv_active);

	RGB16 out_color;

	switch (RenderType)
	{
	case PixelRenderType::SHADED:
		out_color = calculate_pixel_shaded(*col, bar);
		break;
	case PixelRenderType::TEXTURED_PALETTED_4BIT:
		out_color = calculate_pixel_tex_4bit(*tex_info, texel);
		break;
	case PixelRenderType::TEXTURED_16BIT:
		out_color = calculate_pixel_tex_16bit(*tex_info, texel);
		break;
	default:
		printf("Unknown shader type %d\n", (int)RenderType);
		exit(1);
	}

	if ((is_textured || draw_flags.semi_transparency) && out_color.word == 0x0000)
		return;
	
	const auto is_raw = draw_flags.texture_mode == DrawCommand::TextureMode::Raw;

	if (is_textured && !is_raw)
	{
		glm::vec3 brightness;
		if (draw_flags.shading == DrawCommand::Shading::Flat)
			brightness = RGB32::from_word(tex_info->color.word()).to_vec();
		else if (draw_flags.shading == DrawCommand::Shading::Gouraud)
		{
			const auto c = *col;
			brightness = glm::vec3(bar.a * c[0].r + bar.b * c[1].r + bar.c * c[2].r,
                             bar.a * c[0].g + bar.b * c[1].g + bar.c * c[2].g,
                             bar.a * c[0].b + bar.b * c[1].b + bar.c * c[2].b) /
                   (255.f * area);
		}
		out_color *= brightness * 2.f;
	}

	gpu->set_vram_pos(pos.x, pos.y, out_color.word);
}

template <PixelRenderType RenderType>
void Renderer::draw_triangle(Position3 pos, const Color3 *col, const TextureInfo *tex_info, DrawCommand::Flags draw_flags)
{
	auto orient_2d = [](Position a, Position b, Position c) -> int32_t {
		return ((int32_t)b.x - a.x) * ((int32_t)c.y - a.y) - ((int32_t)b.y - a.y) * ((int32_t)c.x - a.x);
	};

	const auto drawing_offset = gpu->drawing_offset;

	for (auto i = 0; i < 3; ++i)
	{
		pos[i].x += drawing_offset.x;
		pos[i].y += drawing_offset.y;
	}

	const auto v0 = pos[0];
	auto v1 = pos[1];
	auto v2 = pos[2];

	const auto area = orient_2d(v0, v1, v2);
	if (!area)
		return;
	
	const auto is_ccw = area < 0;

	if (is_ccw)
		std::swap(v1, v2);
	
	const auto da_left = gpu->drawing_area_top_left.x;
	const auto da_top = gpu->drawing_area_top_left.y;
	const auto da_right = gpu->drawing_area_bottom_right.x;
	const auto da_bottom = gpu->drawing_area_bottom_right.y;
	const int16_t min_x = std::max((int16_t)da_left, std::max((int16_t)0, std::min({ v0.x, v1.x, v2.x })));
	const int16_t min_y = std::max((int16_t)da_top, std::max((int16_t)0, std::min({ v0.y, v1.y, v2.y })));
	const int16_t max_x =
		std::min((int16_t)da_right, std::min((int16_t)VRAM_WIDTH, std::max({ v0.x, v1.x, v2.x })));
	const int16_t max_y =
		std::min((int16_t)da_bottom, std::min((int16_t)VRAM_HEIGHT, std::max({ v0.y, v1.y, v2.y })));

	// Rasterize
	Position p_iter;
	for (p_iter.y = min_y; p_iter.y < max_y; p_iter.y++)
	{
		for (p_iter.x = min_x; p_iter.x < max_x; p_iter.x++)
		{
			const auto w0 = orient_2d(v1, v2, p_iter);
			auto w1 = orient_2d(v2, v0, p_iter);
			auto w2 = orient_2d(v0, v1, p_iter);

			if (w0 >= 0 && w1 >= 0 && w2 >= 0)
			{
				if (is_ccw)
					std::swap(w1, w2);
				const auto area_abs = std::abs(area);
				const auto bar = BarycentricCoords{w0, w1, w2};
				draw_pixel<RenderType>(p_iter, col, tex_info, bar, area_abs, draw_flags);
			}
		}
	}
}

void Renderer::draw_polygon(const DrawCommand::Polygon &polygon)
{
	const auto gp0 = gpu->get_gp0();

	Position4 positions{};
	Color4 colors{};
	TextureInfo tex_info{};

	extract_draw_data_polygon(polygon, gp0, positions, colors, tex_info);

	draw_polygon_impl(positions, colors, tex_info, polygon.is_quad(), *(DrawCommand::Flags*)&polygon);
}

PixelRenderType tex_page_col_to_render_type(uint8_t tex_page_colors) {
  switch (tex_page_colors) {
    case 0: return PixelRenderType::TEXTURED_PALETTED_4BIT;
    case 1: return PixelRenderType::TEXTURED_PALETTED_8BIT;
    case 2: return PixelRenderType::TEXTURED_16BIT;
    case 3: return PixelRenderType::TEXTURED_16BIT;
  }
  return PixelRenderType::SHADED;  // unreachable
}

void Renderer::draw_polygon_impl(Position4 positions, Color4 colors, TextureInfo tex_info, bool is_quad, DrawCommand::Flags draw_flags)
{
	 const auto end_tri_idx = is_quad ? QuadTriangleIndex::Second : QuadTriangleIndex::First;
	const auto is_textured = draw_flags.texture_mapped;

	const auto texpage = Gp0DrawMode{ tex_info.page };
	auto pixel_render_type = tex_page_col_to_render_type(texpage.tex_page_colors);

	const Position3 tri_positions_first = { positions[0], positions[1], positions[2] };
	const Color3 tri_colors_first = { colors[0], colors[1], colors[2] };

	const Position3 tri_positions_second = { positions[1], positions[2], positions[3] };
	const Color3 tri_colors_second = { colors[1], colors[2], colors[3] };

	Position3 tri_positions = tri_positions_first;
	Color3 tri_colors = tri_colors_first;

	QuadTriangleIndex tri_idx = QuadTriangleIndex::First;
	while (tri_idx <= end_tri_idx) {
		if (is_textured) {
			if (tri_idx == QuadTriangleIndex::Second)
				tri_positions = tri_positions_second;
			tex_info.update_active_triangle(tri_idx);
			draw_triangle_textured(tri_positions, &tri_colors, tex_info, draw_flags, pixel_render_type);
		} else {                                       // Non-textured
			if (tri_idx == QuadTriangleIndex::Second) {  // rendering second triangle
				tri_positions = tri_positions_second;
				tri_colors = tri_colors_second;
			}
			draw_triangle<PixelRenderType::SHADED>(tri_positions, &tri_colors, nullptr, draw_flags);
		}

		tri_idx = (QuadTriangleIndex)((uint32_t)tri_idx + 1);
	}
}

void Renderer::extract_draw_data_polygon(const DrawCommand::Polygon &polygon, const std::vector<uint32_t> &gp0_cmd, Position4 &positions, Color4 &colors, TextureInfo &tex_info) const
{
	const auto vertex_count = polygon.get_vertex_count();
	uint8_t arg_index = 1;

	for (auto v_idx = 0; v_idx < vertex_count; ++v_idx)
	{
		positions[v_idx] = Position::from_gp0(gp0_cmd[arg_index++]);

		if (polygon.texture_mode == DrawCommand::TextureMode::Blended && (polygon.shading == DrawCommand::Shading::Flat || v_idx == 0))
			colors[v_idx] = Color::from_gp0(gp0_cmd[0]);
		if (polygon.texture_mapping)
		{
			if (v_idx == 0)
				tex_info.palette = Palette::from_gp0(gp0_cmd[arg_index]);
			if (v_idx == 1)
				tex_info.page = (gp0_cmd[arg_index] >> 16) & 0xFFFF;
			tex_info.uv[v_idx] = Texcoord::from_gp0(gp0_cmd[arg_index++]);
		}
		if (polygon.shading == DrawCommand::Shading::Gouraud && v_idx < vertex_count - 1)
			colors[v_idx + 1] = Color::from_gp0(gp0_cmd[arg_index++]);
	}
	tex_info.color = colors[0];
}

void Renderer::Renderer::draw_triangle_textured(Position3 tri_positions, const Color3 *col, TextureInfo tex_info, DrawCommand::Flags draw_flags, PixelRenderType pixel_render_type)
{
	switch (pixel_render_type) {
    case PixelRenderType::TEXTURED_PALETTED_4BIT:
      draw_triangle<PixelRenderType::TEXTURED_PALETTED_4BIT>(tri_positions, col, &tex_info, draw_flags);
      break;
    case PixelRenderType::TEXTURED_PALETTED_8BIT:
      draw_triangle<PixelRenderType::TEXTURED_PALETTED_8BIT>(tri_positions, col, &tex_info, draw_flags);
      break;
    case PixelRenderType::TEXTURED_16BIT:
      draw_triangle<PixelRenderType::TEXTURED_16BIT>(tri_positions, col, &tex_info, draw_flags);
      break;
  }
}

RGB16 Renderer::Renderer::calculate_pixel_shaded(Color3 colors, BarycentricCoords bar)
{
	const auto w = (float)(bar.a + bar.b + bar.c);
	const uint8_t r = (uint8_t)((colors[0].r * bar.a + colors[1].r * bar.b + colors[2].b * bar.c) / w);
	const uint8_t g = (uint8_t)((colors[0].g * bar.a + colors[1].g * bar.b + colors[2].g * bar.c) / w);
	const uint8_t b = (uint8_t)((colors[0].b * bar.a + colors[1].b * bar.b + colors[2].b * bar.c) / w);
	return RGB16::from_RGB(r, g, b);
}
TexelPos Renderer::Renderer::calculate_texel_pos(BarycentricCoords bar, int32_t area, Texcoord3 uv) const
{
	TexelPos texel;

	texel.x = (int32_t)(bar.a * uv[0].x + bar.b * uv[1].x + bar.c * uv[2].x) / area;
	texel.y = (int32_t)(bar.a * uv[0].y + bar.b * uv[1].y + bar.c * uv[2].y) / area;

	texel.x %= 256;
	texel.y %= 256;

	const auto tex_win = gpu->tex_window;
	texel.x = (texel.x & ~(tex_win.tex_window_mask_x * 8)) |
            ((tex_win.tex_window_off_x & tex_win.tex_window_mask_x) * 8);
	texel.y = (texel.y & ~(tex_win.tex_window_mask_y * 8)) |
				((tex_win.tex_window_off_y & tex_win.tex_window_mask_y) * 8);
	return texel;
}
RGB16 Renderer::Renderer::calculate_pixel_tex_4bit(TextureInfo tex_info, TexelPos texel_pos) const
{
	const auto texpage = Gp0DrawMode{tex_info.page};

	const auto index_x = texel_pos.x / 4 + texpage.tex_base_x();
	const auto index_y = texel_pos.y + texpage.tex_base_y();

	const uint16_t index = index_x + index_y * VRAM_WIDTH;

	const auto index_shift = (texel_pos.x & 0b11) * 4;
	const uint16_t entry = (index >> index_shift) & 0xF;

	const auto clut_x = tex_info.palette.x() + entry;
  	const auto clut_y = tex_info.palette.y();

	const uint16_t color = gpu->GetVram()[clut_x + clut_y * VRAM_WIDTH];

	return RGB16::from_word(color);
}
RGB16 Renderer::calculate_pixel_tex_16bit(TextureInfo tex_info, TexelPos texel_pos) const
{
	const auto texpage = Gp0DrawMode{tex_info.page};

	const auto color_x = texel_pos.x + texpage.tex_base_x();
	const auto color_y = texel_pos.y + texpage.tex_base_y();

	uint16_t color = gpu->GetVram()[color_x + color_y * VRAM_WIDTH];

	return RGB16::from_word(color);
}
}
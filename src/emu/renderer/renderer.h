#pragma once

#include <algorithm>
#include <array>
#include "shader.h"

#include <SDL2/SDL.h>

class GPU;

namespace renderer
{

enum class QuadTriangleIndex
{
	None,
	First,
	Second
};

struct Color;
struct Position;
struct Texcoord;

using Color3 = std::array<Color, 3>;
using Color4 = std::array<Color, 4>;
using Position3 = std::array<Position, 3>;
using Position4 = std::array<Position, 4>;
using Texcoord3 = std::array<Texcoord, 3>;
using Texcoord4 = std::array<Texcoord, 4>;

struct Position
{
	int16_t x;
	int16_t y;

	static Position from_gp0(uint32_t cmd)
	{
		return {((int16_t)(cmd & 0x7FF)), ((int16_t)((cmd >> 16) & 0x7FF))};
	}

	static Position from_gp0_fill(uint32_t cmd) {return {(int16_t)(cmd & 0x3F0), (int16_t)((cmd >> 16) & 0x3F0)};}
	static Position3 from_gp0(uint32_t cmd, uint32_t cmd2, uint32_t cmd3) {
		return { from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3) };
	}
	static Position4 from_gp0(uint32_t cmd, uint32_t cmd2, uint32_t cmd3, uint32_t cmd4) {
		return { from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3), from_gp0(cmd4) };
	}
	Position operator+(const Position& rhs) const {
		return { (int16_t)(x + rhs.x), (int16_t)(y + rhs.y) };
	}
};

struct Size
{
	int16_t width;
	int16_t height;

	static Size from_gp0(uint32_t cmd)
	{
		return {((int16_t)(cmd & 0x1FF)), ((int16_t)((cmd >> 16) & 0x3FF))};
	}
	static Size from_gp0_fill(uint32_t cmd)
	{
		return { (((int16_t)(cmd & 0x3FF) + 0xF) & ~0xF, (int16_t)((cmd >> 16) & 0x1FF))};
	}
};

enum class PixelRenderType
{
	SHADED,
	TEXTURED_PALETTED_4BIT,
	TEXTURED_PALETTED_8BIT,
	TEXTURED_16BIT
};

struct Texcoord {
  int16_t x;
  int16_t y;

  static Texcoord from_gp0(uint32_t cmd) { return { (uint8_t)(cmd & 0xFF), (uint8_t)((cmd >> 8) & 0xFF) }; }
  static Texcoord4 from_gp0(uint32_t cmd1, uint32_t cmd2, uint32_t cmd3, uint32_t cmd4) {
    return { from_gp0(cmd1), from_gp0(cmd2), from_gp0(cmd3), from_gp0(cmd4) };
  }
  Texcoord operator+(const Texcoord& rhs) const {
    // TODO: sign extend?
    return { (int16_t)(x + rhs.x), (int16_t)(y + rhs.y) };
  }
};

union Palette
{
	struct
	{
		uint16_t _x : 6;
		uint16_t _y : 9;
		uint16_t _pad : 1;
	};
	static Palette from_gp0(uint32_t cmd)
	{
		Palette p;
		p.word = (cmd >> 16) & 0xffff;
		return p;
	}

	uint16_t x() const {return _x * 16;}
	uint16_t y() const {return _y;}

	uint16_t word;
};

struct Color
{
	uint8_t r, g, b;

	static Color from_gp0(uint32_t cmd) {return {(uint8_t)cmd, (uint8_t)(cmd >> 8), (uint8_t)(cmd >> 16)};}
	static Color3 from_gp0(uint32_t cmd, uint32_t cmd2, uint32_t cmd3)
	{
		return {from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3)};
	}
	static Color4 from_gp0(uint32_t cmd, uint32_t cmd2, uint32_t cmd3, uint32_t cmd4)
	{
		return {from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3), from_gp0(cmd4)};
	}

	uint32_t word() const { return r | g << 8 | b << 16; }
};

struct TextureInfo
{
	Texcoord4 uv;
	Texcoord3 uv_active;
	Palette palette;
	uint16_t page;
	Color color;

	void update_active_triangle(QuadTriangleIndex triangles_index)
	{
		switch (triangles_index)
		{
		case QuadTriangleIndex::First: uv_active = {uv[0], uv[1], uv[2]}; break;
		case QuadTriangleIndex::Second: uv_active = {uv[1], uv[2], uv[3]}; break;
		case QuadTriangleIndex::None: break;
		}
	}
};

union DrawCommand {
  enum class TextureMode : uint8_t {
    Blended = 0,
    Raw = 1,
  };
  enum class RectSize : uint8_t {
    SizeVariable = 0,
    Size1x1 = 1,
    Size8x8 = 2,
    Size16x16 = 3,
  };
  enum class VertexCount : uint8_t {
    Triple = 0,
    Quad = 1,
  };
  enum class LineCount : uint8_t {
    Single = 0,
    Poly = 1,
  };
  enum class Shading : uint8_t {
    Flat = 0,
    Gouraud = 1,
  };
  enum class PrimitiveType : uint8_t {
    Polygon = 1,
    Line = 2,
    Rectangle = 3,
  };

  uint8_t word{};

  struct Line {
    uint8_t : 1;
    uint8_t semi_transparency : 1;
    uint8_t : 1;
    LineCount line_count : 1;
    Shading shading : 1;
    uint8_t : 3;

    bool is_poly() const { return line_count == LineCount::Poly; }
    uint8_t get_arg_count() const {
      if (is_poly())
        return 32 - 1;
      return 2 + (shading == Shading::Gouraud ? 1 : 0);
    }

  } line;

  struct Rectangle {
    TextureMode texture_mode : 1;
    uint8_t semi_transparency : 1;
    uint8_t texture_mapping : 1;
    RectSize rect_size : 2;
    uint8_t : 3;

    bool is_variable_sized() const { return rect_size == RectSize::SizeVariable; }
    Size get_static_size() const {
      switch (rect_size) {
        case RectSize::Size1x1: return { 1, 1 };
        case RectSize::Size8x8: return { 8, 8 };
        case RectSize::Size16x16: return { 16, 16 };
        case RectSize::SizeVariable:
        default: printf("[emu/Renderer]: Invalid size\n"); return { 0, 0 };
      }
    }
    uint8_t get_arg_count() const {
      uint8_t arg_count = 1;

      if (is_variable_sized())
        arg_count += 1;

      if (texture_mapping)
        arg_count += 1;
      return arg_count;
    }
  } rectangle;

  struct Polygon {
    TextureMode texture_mode : 1;
    uint8_t semi_transparency : 1;
    uint8_t texture_mapping : 1;
    VertexCount vertex_count : 1;
    Shading shading : 1;
    uint8_t : 3;

    bool is_quad() const { return vertex_count == VertexCount::Quad; }
    uint8_t get_vertex_count() const { return is_quad() ? 4 : 3; }
    uint8_t get_arg_count() const {
      uint8_t arg_count = get_vertex_count();

      if (texture_mapping)
        arg_count *= 2;

      if (shading == Shading::Gouraud)
        arg_count += get_vertex_count() - 1;
      return arg_count;
    }
  } polygon;

  struct Flags {
    TextureMode texture_mode : 1;
    uint8_t semi_transparency : 1;
    uint8_t texture_mapped : 1;
    uint8_t : 1;
    Shading shading : 1;
  } flags;
};

struct BarycentricCoords
{
	int32_t a;
	int32_t b;
	int32_t c;
};

union RGB16
{
	struct
	{
		uint16_t r : 5;
		uint16_t g : 5;
		uint16_t b : 5;
		uint16_t mask : 1;
	};

	uint16_t word;

	static RGB16 from_RGB(uint8_t r, uint8_t g, uint8_t b)
	{
		RGB16 c16{};
		c16.r = r >> 3;
		c16.g = g >> 3;
		c16.b = b >> 3;
		c16.mask = 0;
		return c16;
	}

	static RGB16 from_word(uint16_t word)
	{
		RGB16 rgb16;
		rgb16.word = word;
		return rgb16;
	}

	RGB16 operator*(const glm::vec3& rhs) {
		r = std::min<uint16_t>(uint16_t(r * rhs.r), 31);
		g = std::min<uint16_t>(uint16_t(g * rhs.g), 31);
		b = std::min<uint16_t>(uint16_t(b * rhs.b), 31);
		return *this;
	}
	RGB16 operator*=(const glm::vec3& rhs) { return *this * rhs; }
};

union RGB32 {
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t _pad;
  };
  uint32_t word{};

  static RGB32 from_word(uint32_t word) {
    RGB32 rgb32;
    rgb32.word = word;
    return rgb32;
  }

  glm::vec3 to_vec() const { return { r / 255.f, g / 255.f, b / 255.f }; }
};


struct TexelPos {
  int32_t x;
  int32_t y;
};

class Renderer
{
public:
	GPU* gpu;

	Renderer();

	void render(const void* vram_data) const;
	void bind_screen_texture() const;
	void set_texture_size(int32_t width, int32_t height);

	template<PixelRenderType RenderType>
	void draw_pixel(Position pos, const Color3* col, const TextureInfo* tex_info, BarycentricCoords coords, int32_t area, DrawCommand::Flags draw_flags);
	template<PixelRenderType RenderType>
	void draw_triangle(Position3 pos, const Color3* col, const TextureInfo* tex_info, DrawCommand::Flags draw_flags);
	void draw_polygon(const DrawCommand::Polygon& polygon);
private:
	void draw_polygon_impl(Position4 positions,
                         Color4 colors,
                         TextureInfo tex_info,
                         bool is_quad,
                         DrawCommand::Flags draw_flags);
	void extract_draw_data_polygon(const DrawCommand::Polygon& polygon,
                                 const std::vector<uint32_t>& gp0_cmd,
                                 Position4& positions,
                                 Color4& colors,
                                 TextureInfo& tex_info) const;

	void draw_triangle_textured(Position3 tri_positions,
                              const Color3* col,
                              TextureInfo tex_info,
                              DrawCommand::Flags draw_flags,
                              PixelRenderType pixel_render_type);
	
	void draw_rectangle(const DrawCommand::Rectangle& polygon);

	static RGB16 calculate_pixel_shaded(Color3 colors, BarycentricCoords bar);

	TexelPos calculate_texel_pos(BarycentricCoords bar, int32_t area, Texcoord3 uv) const;

	RGB16 calculate_pixel_tex_4bit(TextureInfo tex_info, TexelPos texel_pos) const;
	RGB16 calculate_pixel_tex_8bit(TextureInfo tex_info, TexelPos texel_pos) const;
	RGB16 calculate_pixel_tex_16bit(TextureInfo tex_info, TexelPos texel_pos) const;

	int32_t screen_width, screen_height;
	gl::GLuint shader_program;

	gl::GLuint vao;
	gl::GLuint vbo;
	gl::GLuint tex_screen;
	gl::GLuint tex_size;

	SDL_Window* window;
	SDL_GLContext context;
};

}
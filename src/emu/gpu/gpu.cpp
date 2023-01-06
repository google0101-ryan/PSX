#include "gpu.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <emu/renderer/renderer.h>

extern renderer::Renderer* g_renderer;

union ClutAttrib {
	ushort raw;

	struct {
		ushort x : 6;
		ushort y : 9;
		ushort zr : 1;
	};
};

union TPageAttrib {
	ushort raw;

	struct {
		ushort page_x : 4;
		ushort page_y : 1;
		ushort semi_transp : 2;
		ushort page_colors : 2;
		ushort : 7;
	};
};

void GPU::HandleGP0(uint32_t command)
{
	if (mode == WAITING_ON_COMMAND)
	{
		uint8_t cmd = (command >> 24) & 0xff;
		switch ((command >> 24) & 0xff)
		{
		case 0x00:
		case 0x01:
		case 0x02:
			break;
		case 0x20 ... 0x3F:
			mode = WAITING_PARAMS;
			param_count = renderer::DrawCommand{cmd}.polygon.get_arg_count();
			parameters.push_back(command);
			break;
		case 0x60 ... 0x7F:
			mode = WAITING_PARAMS;
			param_count = renderer::DrawCommand{cmd}.rectangle.get_arg_count();
			parameters.push_back(command);
			break;
		case 0xA0:
			mode = WAITING_PARAMS;
			param_count = 3;
			parameters.push_back(command);
			break;
		case 0xC0:
			mode = WAITING_PARAMS;
			param_count = 3;
			parameters.push_back(command);
			break;
		case 0xE1:
			draw_mode.word = command;

			draw_mode.rect_textured_x_flip = (command & (1 << 12)) >> 12;
			draw_mode.rect_textured_y_flip = (command & (1 << 13)) >> 13;
			break;
		case 0xE2:
			tex_window.word = command;
			break;
		case 0xE3:
			drawing_area_top_left.word = command;
			break;
		case 0xE4:
			drawing_area_bottom_right.word = command;
			break;
		case 0xE5:
			drawing_offset.word = command;
			break;
		case 0xE6:
			break;
		default:
			printf("[emu/GPU]: Unhandled GP0 command 0x%08x\n", command);
			exit(1);
		}
	}
	else if (mode == WAITING_PARAMS)
	{
		parameters.push_back(command);
		if (parameters.size() == param_count)
		{
			mode = WAITING_ON_COMMAND;
			switch ((parameters[0] >> 24) & 0xff)
			{
			case 0x20 ... 0x3F:
			{
				const uint8_t opcode = parameters[0] >> 24;
				auto polygon = renderer::DrawCommand{opcode}.polygon;
				g_renderer->draw_polygon(polygon);
				break;
			}
			case 0x60 ... 0x7F:
			{
				const uint8_t opcode = parameters[0] >> 24;
				auto rectangle = renderer::DrawCommand{opcode}.rectangle;
				break;
			}
			case 0xA0:
			{
				const auto pos_word = parameters[1];
				const auto size_word = parameters[2];

				cur_transfer_x = pos_word & 0x3FF;
				cur_transfer_y = (pos_word >> 16) & 0x1FF;
				cur_transfer_start_x = cur_transfer_x;

				cur_transfer_width = (((size_word & 0xFFFF) - 1) & 0x3FF) + 1;
				cur_transfer_height = ((((size_word >> 16) & 0xFFFF) - 1) & 0x1FF) + 1;

				const auto pixel_count = (cur_transfer_width * cur_transfer_height + 1) & ~1u;
				image_remaining = pixel_count;

				mode = IMAGE_TRANSFER_TO_VRAM;
				break;
			}
			case 0xC0:
			{
				const auto pos_word = parameters[1];
				const auto size_word = parameters[2];

				cur_transfer_x = pos_word & 0x3FF;
				cur_transfer_y = (pos_word >> 16) & 0x1FF;
				cur_transfer_start_x = cur_transfer_x;

				cur_transfer_width = (((size_word & 0xFFFF) - 1) & 0x3FF) + 1;
				cur_transfer_height = ((((size_word >> 16) & 0xFFFF) - 1) & 0x1FF) + 1;

				const auto pixel_count = (cur_transfer_width * cur_transfer_height + 1) & ~1u;
				image_remaining = pixel_count;

				// mode = IMAGE_TRANSFER_FROM_VRAM;
				break;
			}
			default:
				printf("[emu/GPU]: Unhandled GP0 w/ parameters command 0x%02x\n", (parameters[0] >> 24) & 0xff);
				exit(1);
			}
			parameters.clear();
			param_count = 0;
		}
	}
	else if (mode == IMAGE_TRANSFER_TO_VRAM)
	{
		for (int i = 0; i < 2; ++i)
		{
			uint16_t src_word;
			if (i == 0)
				src_word = (uint16_t)command;
			else
				src_word = (uint16_t)(command >> 16);
			
			set_vram_pos(cur_transfer_x, cur_transfer_y, src_word);
			const auto rect_x = cur_transfer_x - cur_transfer_start_x;
			if (rect_x == cur_transfer_width - 1)
			{
				cur_transfer_x = cur_transfer_start_x;
				cur_transfer_y++;
			}
			else
				cur_transfer_x++;
			image_remaining--;
		}
		if (!image_remaining)
			mode = WAITING_ON_COMMAND;
	}
}

void GPU::HandleGP1(uint32_t command)
{
	switch ((command >> 24) & 0xff)
	{
	case 0x00:
		gpustat = GpuStatus();
		draw_mode = Gp0DrawMode();
		break;
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
		break;
	case 0x05:
		display_area.word = command;
		break;
	case 0x06:
		hdisplay_range.word = command;
		break;
	case 0x07:
		vdisplay_range.word = command;
		break;
	case 0x08:
	{
		break;
	}
	default:
		printf("[emu/GPU]: Unhandled GP1 command 0x%08x\n", command);
		exit(1);
	}
}

glm::ivec2 GPU::PosFromGP0(uint32_t word)
{
	int16_t x = word & 0xffff;
	int16_t y = (word >> 16) & 0xffff;

	return glm::ivec2(x, y);
}

glm::ivec3 GPU::ColFromGP0(uint32_t word)
{
	uint8_t r = word & 0xff;
	uint8_t g = (word >> 8) & 0xff;
	uint8_t b = (word >> 16) & 0xff;

	return glm::ivec3{r, g, b};
}

glm::ivec2 GPU::CoordFromGP0(uint32_t word)
{
	glm::ivec2 result;
	result.x = word & 0xff;
	result.y = (word >> 8) & 0xff;
	return result;
}

uint32_t GPU::read_from_vram()
{
	if (image_remaining)
	{
		return 0;
	}
	else
	{
		return 0;
	}
}

GPU::GPU()
{
	gpustat.word = 0x1C802000;
	mode = WAITING_ON_COMMAND;
	m_vram = std::make_unique<std::array<uint16_t, VRAM_WIDTH * VRAM_HEIGHT>>();
}

void GPU::Dump()
{
}

uint32_t GPU::read(uint32_t addr)
{
	//printf("Reading from GPU address 0x%08x\n", addr);

	if (addr == 0x1f801810)
		return read_from_vram();
	if (addr == 0x1f801814)
		return gpustat.word;

	printf("[emu/GPU]: Read from unknown address 0x%08x\n", addr);
	exit(1);
}

void GPU::write(uint32_t addr, uint32_t data)
{
	if (addr == 0x1f801810)
	{
		HandleGP0(data);
		return;
	}
	else if (addr == 0x1f801814)
	{
		HandleGP1(data);
		return;
	}

	printf("[emu/GPU]: Write to unknown address 0x%08x\n", addr);
	exit(1);
}
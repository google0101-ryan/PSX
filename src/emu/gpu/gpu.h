#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <array>

constexpr uint32_t VRAM_WIDTH = 1024;
constexpr uint32_t VRAM_HEIGHT = 512;

union Gp0DrawMode {
  	uint32_t word {};

  	// Only define the fields we want to use (the rest are common with GPUSTAT definition)
	struct 
	{
		uint32_t tex_page_x_base : 4;  //  0-3   Texture page X Base
		uint32_t tex_page_y_base : 1;  //  4     Texture page Y Base
		uint32_t _5_6 : 2;
		uint32_t tex_page_colors : 2;  //  7-8   Texture page colors
		uint32_t _9_11 : 3;
		uint32_t rect_textured_x_flip : 1;  // 12
		uint32_t rect_textured_y_flip : 1;  // 13
	};

	int32_t tex_base_x() const { return tex_page_x_base * 64; }
	int32_t tex_base_y() const { return tex_page_y_base * 256; }
};

class GPU
{
	friend class Renderer;
public:
	uint32_t gpuread = 0;

	enum Mode
	{
		WAITING_ON_COMMAND = 0,
		WAITING_PARAMS = 1,
		IMAGE_TRANSFER_TO_VRAM = 2,
		IMAGE_TRANSFER_FROM_VRAM = 3,
	} mode;

	void HandleGP0(uint32_t command);
	void HandleGP1(uint32_t command);


	
	std::vector<uint32_t> parameters;
	uint32_t param_count = 0;
	uint32_t image_remaining = 0;
	uint32_t cur_transfer_start_x = 0;
	uint16_t cur_transfer_x, cur_transfer_y;
	uint16_t cur_transfer_width, cur_transfer_height = 0;
	uint16_t transfer_x_start, transfer_y_start = 0;

	glm::ivec2 PosFromGP0(uint32_t word);
	glm::ivec3 ColFromGP0(uint32_t word); 
	glm::ivec2 CoordFromGP0(uint32_t word);

	Gp0DrawMode draw_mode;

	union Gp0TextureWindow {
		uint32_t word{};

		struct {
			uint32_t tex_window_mask_x : 5;  //  0-4    Texture window Mask X   (in 8 pixel steps)
			uint32_t tex_window_mask_y : 5;  //  5-9    Texture window Mask Y   (in 8 pixel steps)
			uint32_t tex_window_off_x : 5;   //  10-14  Texture window Offset X (in 8 pixel steps)
			uint32_t tex_window_off_y : 5;   //  15-19  Texture window Offset Y (in 8 pixel steps)
		};
	} tex_window;

	union Gp1DisplayArea {
		uint32_t word{};

		struct {
			uint32_t x : 10;  // X (0-1023)    (halfword address in VRAM)  (relative to begin of VRAM)
			uint32_t y : 9;   // Y (0-511)     (scanline number in VRAM)   (relative to begin of VRAM)
		};
	} display_area;

	union Gp1HDisplayRange {
		uint32_t word{};

		struct {
			uint32_t x1 : 12;  // X1 (260h+0)       ;12bit       ;\counted in 53.222400MHz units
			uint32_t x2 : 12;  // X2 (260h+320*8)   ;12bit       ;/relative to HSYNC
		};
	} hdisplay_range;

		// GP1(07h) - Vertical Display range (on Screen)
	union Gp1VDisplayRange {
		uint32_t word{};

		struct {
			uint32_t y1 : 10;  // Y1 (NTSC=88h-(224/2), (PAL=A3h-(264/2))  ;\scanline numbers on screen
			uint32_t y2 : 10;  // Y2 (NTSC=88h+(224/2), (PAL=A3h+(264/2))  ;/relative to VSYNC
		};
	} vdisplay_range;

	union Gp0DrawingArea {
		uint32_t word{};

		struct {
			uint32_t x : 10;   // 0-9    X-coordinate (0..1023)
			uint32_t y : 9;    // 10-18  Y-coordinate (0..511)   ;\on Old 160pin GPU (max 1MB VRAM)
			uint32_t _19 : 1;  // 19-23  Not used (zero)         ; TODO: Used for y on new GPU
		};
	} drawing_area_top_left, drawing_area_bottom_right;

	union Gp0DrawingOffset {
		uint32_t word{};

		struct {
			uint32_t x : 11;  // 0-10   X-offset (-1024..+1023) (usually within X1,X2 of Drawing Area)
			uint32_t y : 11;  // 11-21  Y-offset (-1024..+1023) (usually within Y1,Y2 of Drawing Area)
		};
	} drawing_offset;

	union GpuStatus 
	{
		uint32_t word{ 0x1C802000 };

		struct {
			uint32_t tex_page_x_base : 4;       //  0-3   Texture page X Base
			uint32_t tex_page_y_base : 1;       //  4     Texture page Y Base
			uint32_t semi_transparency : 2;     //  5-6   Semi Transparency
			uint32_t tex_page_colors : 2;       //  7-8   Texture page colors
			uint32_t dither_en : 1;             //  9     Dither 24bit to 15bit
			uint32_t drawing_to_disp_en : 1;    //  10    Drawing to display area
			uint32_t force_set_mask_bit : 1;    //  11    Set Mask-bit when drawing pixels
			uint32_t preserve_masked_bits : 1;  //  12    Draw Pixels
			uint32_t interlace_field : 1;       //  13    Interlace Field
			uint32_t reverse_flag : 1;          //  14    "Reverseflag"
			uint32_t tex_disable : 1;           //  15    Texture Disable
			uint32_t horizontal_res_2 : 1;      //  16    Horizontal Resolution 2
			uint32_t horizontal_res_1 : 2;      //  17-18 Horizontal Resolut
			uint32_t vertical_res : 1;          //  19    Vertical Resolution
			uint32_t video_mode : 1;            //  20    Video Mode (0=NTSC/60Hz, 1=PAL/50Hz)
			uint32_t disp_color_depth : 1;      //  21    Display Area Color Depth (0=15bit, 1=24bit)
			uint32_t vertical_interlace : 1;    //  22    Vertical Interlace (0=Off, 1=On)
			uint32_t disp_disabled : 1;         //  23    Display Status (0=Enabled, 1=Disabled)
			uint32_t interrupt : 1;             //  24    Interrupt Request (IRQ1) (0=Off, 1=IRQ)
			uint32_t dma_data_req : 1;       //  25    DMA / Data Request, meaning depends on GP1(04h) DMA Direction
			uint32_t ready_to_recv_cmd : 1;  //  26    Ready to receive Cmd Word
			uint32_t ready_to_send_vram_to_cpu : 1;  // 27    Ready to send VRAM to CPU
			uint32_t ready_to_recv_dma_block : 1;    // 28    Ready to receive DMA Block
			uint32_t dma_direction_ : 2;             // 29-30 DMA Direction (0=Off, 1=?, 2=CPUtoGP0, 3=GPUREADtoCPU)
			uint32_t interlace_drawing_mode : 1;     // 31    Drawing even/odd lines in interlace mode (0=Even, 1=Odd)
		};
	} gpustat;

	std::unique_ptr<std::array<uint16_t, VRAM_WIDTH * VRAM_HEIGHT>> m_vram;

	uint32_t read_from_vram();
public:
	void set_vram_pos(uint16_t x, uint16_t y, uint16_t val)
	{
		const auto idx = x + y * VRAM_WIDTH;

		(*m_vram.get())[idx] = val;
	}

	std::array<uint16_t, VRAM_WIDTH*VRAM_HEIGHT>& GetVram()
	{
		return (*m_vram.get());
	}

	std::vector<uint32_t> get_gp0() {return parameters;}

	GPU();
	void Dump();

	uint32_t read(uint32_t addr);
	void write(uint32_t addr, uint32_t data);
};
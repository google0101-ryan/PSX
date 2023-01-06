#pragma once

#include <cstdint>
#include <glm/glm.hpp>

typedef glm::i32vec2 vec2s32;
typedef glm::u32vec3 vec3u32;

class GTE
{
private:
	int16_t zsf3 = 0, zsf4 = 0;
	uint16_t h = 0;
	int16_t dqa = 0;
	int32_t dqb = 0;
	vec2s32 screen_offset = {};
	vec3u32 rgb_bg = {};
	vec3u32 rgb_fc = {};
public:
	void write_reg(uint32_t reg, uint32_t data);
};
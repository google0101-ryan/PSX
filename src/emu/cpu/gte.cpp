#include "gte.h"
#include <cstdio>
#include <cstdlib>

void GTE::write_reg(uint32_t reg, uint32_t data)
{
	switch (reg)
	{
	case 45:
		rgb_bg.r = data;
		break;
	case 46:
		rgb_bg.g = data;
		break;
	case 47:
		rgb_bg.b = data;
		break;
	case 53:
		rgb_fc.r = data;
		break;
	case 54:
		rgb_fc.g = data;
		break;
	case 55:
		rgb_fc.b = data;
		break;
	case 56:
		screen_offset.x = (int32_t)data;
		break;
	case 57:
		screen_offset.y = (int32_t)data;
		break;
	case 58:
		h = (uint16_t)data;
		break;
	case 59:
		dqa = (int16_t)data;
		break;
	case 60:
		dqb = (int32_t)data;
		break;
	case 61:
		zsf3 = (int16_t)data;
		break;
	case 62:
		zsf4 = (int16_t)data;
		break;
	default:
		printf("[emu/GTE]: Write to unknown register %d\n", reg);
		exit(1);
	}
}
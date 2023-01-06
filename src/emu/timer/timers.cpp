#include "timers.h"
#include <emu/memory/Bus.h>

uint8_t timer_from_addr(uint32_t addr)
{
	const auto timer_select = (addr & 0xF0) >> 4;
	return timer_select;
}

void Timers::step(uint32_t cycles)
{
	const uint16_t timer_inc[3] = {cycles, cycles, source2() ? cycles / 8 : cycles};

	for (auto i = 0; i < 3; i++)
	{
		if (timer_paused[i])
			continue;
		
		auto& value = timer_value[i];
		auto& mode = timer_modes[i];
		auto& target = timer_target[i];

		value += timer_inc[i];

		bool could_irq = false;

		if (value > target)
		{
			mode.reached_target = true;
			if (mode.irq_on_target)
				could_irq = true;
			if (mode.reset_on_target)
				value = 0;
		}

		if (value > 0xFFFF)
		{
			mode.reached_max = true;
			if (mode.irq_on_max)
				could_irq = true;
			if (!mode.reset_on_target)
				value = 0;
		}

		if (could_irq)
		{
			if (mode.irq_toggle_mode() == TimerMode::ToggleMode::Toggle)
				mode.irq_not ^= 1;
			else
				mode.irq_not = false;
			
			if (mode.irq_repeat_mode() != TimerMode::RepeatMode::Once && !timer_irq_occured[i])
			{
				if (!mode.irq_not)
				{
					bus->TriggerInterrupt(4 + i);
					timer_irq_occured[i] = true;
				}
				mode.irq_not = true;
			}
		}

		value = static_cast<uint16_t>(value);
	}
}

uint16_t Timers::read(uint32_t addr)
{
	uint8_t timer_select = timer_from_addr(addr);
	uint8_t reg = addr & 0xF;

	auto& value = timer_value[timer_select];
	auto& mode = timer_modes[timer_select];
	auto& target = timer_target[timer_select];

	switch (reg)
	{
	case 0:
		return value;
	case 4:
		return mode.read();
	case 8:
		return target;
	}
}

void Timers::write(uint32_t addr, uint16_t data)
{
	uint8_t timer_select = timer_from_addr(addr);
	uint8_t reg = addr & 0xF;

	auto& value = timer_value[timer_select];
	auto& mode = timer_modes[timer_select];
	auto& target = timer_target[timer_select];

	switch (reg)
	{
	case 0:
		value = data;
		break;
	case 4:
		mode.word = data;
		mode.irq_not = true;

		timer_paused[timer_select] = false;
		timer_irq_occured[timer_select] = false;

		value = 0;

		if (mode.sync_enable)
		{
			if (timer_select == 2)
				if (mode.sync_mode == 0 || mode.sync_mode == 3)
					timer_paused[timer_select] = true;
		}
		break;
	case 8:
		target = data;
		break;
	}
}

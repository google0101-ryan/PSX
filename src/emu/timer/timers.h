#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

class Bus;

class Timers
{
private:
	union TimerMode
	{
		enum class RepeatMode : bool
		{
			Once = 0,
			Repeat = 1,
		};
		enum class ToggleMode : bool
		{
			Pulse = 0,
			Toggle = 1,
		};

		uint16_t word;

		struct
		{
			uint16_t sync_enable : 1,
			sync_mode : 2,
			reset_on_target : 1,

			irq_on_target : 1,
			irq_on_max : 1,
			_irq_repeat_mode : 1,
			_irq_toggle_mode : 1,
			clock_source : 2,
			irq_not : 1,
			reached_target : 1,
			reached_max : 1;
		};

		uint16_t read()
		{
			auto reg_val = word;
			reached_target = false;
			reached_max = false;
			return reg_val;
		}
		RepeatMode irq_repeat_mode() const { return static_cast<RepeatMode>(_irq_repeat_mode); }
  		ToggleMode irq_toggle_mode() const { return static_cast<ToggleMode>(_irq_toggle_mode); }
	};

	uint32_t timer_value[3] = {};
	TimerMode timer_modes[3] = {};
	uint16_t timer_target[3] = {};

	bool timer_paused[3] = {};
	bool timer_irq_occured[3] = {};

	Bus* bus;

	bool source2() const {
		return timer_modes[2].clock_source >= 2;
	}
public:
	Timers(Bus* bus) : bus(bus) {}

	void step(uint32_t cycles);

	uint16_t read(uint32_t addr);
	void write(uint32_t addr, uint16_t data);
};
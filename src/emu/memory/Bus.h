#pragma once

#include <cstdint>
#include <string>
#include <emu/dma/dma.h>
#include <emu/gpu/gpu.h>
#include <emu/cdvd/cdvd.h>
#include <emu/timer/timers.h>

class CPU;

class Bus
{
	friend class System;
private:
	uint8_t bios[0x80000];
	uint8_t ram[0x200000];

	static uint32_t Translate(uint32_t addr)
    {
        constexpr uint32_t KUSEG_MASKS[8] =
        {
            /* KUSEG: Don't touch the address, it's fine */
            0xffffffff, 0xfffffff, 0xfffffff, 0xffffffff,
            /* KSEG0: Strip the MSB (0x8 -> 0x0 and 0x9 -> 0x1) */
            0x7fffffff,
            /* KSEG1: Strip the 3 MSB's (0xA -> 0x0 and 0xB -> 0x1) */
            0x1fffffff,
            /* KSEG2: Don't touch the address, it's fine */
            0xffffffff, 0x1fffffff,
        };

        addr &= KUSEG_MASKS[addr >> 29];

        return addr;
	}

	DMA* dma;
	GPU* gpu;
	CDVD* dvd;
	Timers* timers;
public:
	uint32_t I_MASK = 0;
	uint32_t I_STAT = 0;

	void step() 
	{
		timers->step(300);
		dvd->step();
	}

	GPU* get_gpu() {return gpu;}

	Bus(std::string biosFileName);
	void Dump();

	void TriggerInterrupt(int interrupt);

	void LoadEXE(std::string exe_name, CPU* cpu);

	template<typename T>
	T read(uint32_t addr)
	{
		addr = Translate(addr);

		
		if (addr < 0x200000)
			return *(T*)&ram[addr];
		if (addr >= 0x1fc00000 && addr < 0x1fc80000)
			return *(T*)&bios[addr - 0x1fc00000];


		if ((addr & 0xfffffff0) == 0x1f801100 || (addr & 0xfffffff0) == 0x1f801110 || (addr & 0xfffffff0) == 0x1f801120)
			return timers->read(addr);
		if (addr >= 0x1f000000 && addr < 0x1f080000)
			return 0;
		if (addr >= 0x1f801d80 && addr <= 0x1f801dfc)
			return 0;
		if (addr >= 0x1f801c00 && addr <= 0x1F801D7E)
			return 0;

		switch (addr)
		{
		case 0x1f80101c:
			return 0;
		case 0x1f801070:
			return I_STAT;
		case 0x1f801074:
			return I_MASK;
		case 0x1f8010f0:
		case 0x1f8010f4:
			return dma->read(addr);
		case 0x1f8010A0 ... 0x1f8010AF:
		case 0x1f8010E0 ... 0x1f8010EF:
			return dma->read_dma(addr);
		case 0x1f801810:
		case 0x1f801814:
			return gpu->read(addr);
		case 0x1f801800 ... 0x1f801803:
			return dvd->read(addr);
		case 0x1f801040:
			return 0xFF;
		case 0x1f801044:
			return 0x7;
		case 0x1f80104A:
			return 0;
		}
		
		printf("[emu/Bus]: Read from unknown address 0x%08x\n", addr);
		exit(1);
	}
	
	template<typename T>
	void write(uint32_t addr, T data)
	{
		addr = Translate(addr);

		if (addr < 0x200000)
		{
			*(T*)&ram[addr] = data;
			return;
		}

		if (addr >= 0x1f801000 && addr <= 0x1f801020 || addr == 0x1f801060 || addr == 0x1ffe0130)
			return;

		if (addr >= 0x1f801d80 && addr <= 0x1f801dbc)
			return;

		if (addr == 0x1f802041)
		{
			printf("TraceStep (%x)\n", data & 0xFF);
			return;
		}

		if ((addr & 0xfffffff0) == 0x1f801100 || (addr & 0xfffffff0) == 0x1f801110 || (addr & 0xfffffff0) == 0x1f801120)
		{
			timers->write(addr, data);
			return;
		}
		if (addr >= 0x1f801c00 && addr <= 0x1F801D7E)
			return;
		if (addr >= 0x1f801d80 && addr <= 0x1f801dff)
			return;

		switch (addr)
		{
		case 0x1f801070:
			I_STAT &= data;
			return;
		case 0x1f801074:
			I_MASK = data;
			printf("Writing 0x%08x to I_MASK\n", data);
			return;
		case 0x1f8010f0:
		case 0x1f8010f4:
			dma->write(addr, data);
			return;
		case 0x1f8010A0 ... 0x1f8010AF:
		case 0x1f8010E0 ... 0x1f8010EF:
			dma->write_dma(addr, data);
			return;
		case 0x1f801810:
		case 0x1f801814:
			gpu->write(addr, data);
			return;
		case 0x1f801040:
		case 0x1f801044:
		case 0x1f801048:
		case 0x1f80104a:
		case 0x1f80104e:
			return;
		case 0x1f801800 ... 0x1f801803:
			dvd->write(addr, data);
			return;
		case 0x1f802082:
			printf("PCSX test exited with code %d\n", data);
			exit(1);
		}
		
		printf("[emu/Bus]: Write to unknown address 0x%08x\n", addr);
		exit(1);
	}
};
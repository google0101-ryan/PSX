#include "dma.h"
#include <emu/memory/Bus.h>
#include <cstdio>
#include <cstdlib>

DMA::DMA(Bus* bus)
: bus(bus)
{
	dpcr = 0x07654321;
	dicr = 0;

	channels[2].RunFunc = std::bind(&DMA::HandleGPU, this);
	channels[6].RunFunc = std::bind(&DMA::HandleOTC, this);
}

uint32_t DMA::read(uint32_t addr)
{
	switch (addr)
	{
	case 0x1f8010f0:
		return dpcr;
	case 0x1f8010f4:
		return dicr;
	default:
		printf("[emu/DMA]: Read from unknown address 0x%08x\n", addr);
		exit(1);
	}
}

void DMA::write(uint32_t addr, uint32_t data)
{
	switch (addr)
	{
	case 0x1f8010f0:
		//printf("Writing 0x%08x to DPCR\n", data);
		dpcr = data;
		break;
	case 0x1f8010f4:
		dicr = data;
		break;
	default:
		printf("[emu/DMA]: Write to unknown address 0x%08x\n", addr);
		exit(1);
	}
}

const char* REGS[] =
{
	"MADR",
	"BCR",
	"CHCR",
};

void DMA::write_dma(uint32_t addr, uint32_t data)
{
	int channel = ((addr >> 4) & 0xf) - 0x8;
	int reg = addr & 0xf;

	//printf("Writing 0x%08x to %s of channel %d\n", data, REGS[reg / 4], channel);

	switch (reg)
	{
	case 0x0:
		channels[channel].madr = data;
		return;
	case 0x4:
		channels[channel].bcr.value = data;
		break;
	case 0x8:
		channels[channel].chcr.value = data;
		if ((channels[channel].chcr.start || channels[channel].chcr.busy) && dpcr & (1 << ((4 * channel) + 3)))
			if (!channels[channel].RunFunc)
			{
				printf("[emu/DMA]: Unhandled transfer on channel %d\n", channel);
				exit(1);
			}
			else
				channels[channel].RunFunc();
		return;
	default:
		printf("[emu/DMA]: Write to unknown address 0x%08x\n", addr);
		exit(1);
	}
}

void DMA::HandleOTC()
{
	auto& chan = channels[6];

	if (chan.chcr.sync_mode != 0)
	{
		printf("Unhandled OTC sync mode %d\n", chan.chcr.sync_mode);
		exit(1);
	}

	uint32_t addr = chan.madr;
	uint32_t remsz = chan.bcr.value;
	//printf("[emu/DMA]: Doing transfer of size %d words on channel 6\n", remsz);

	int inc = chan.chcr.address_step ? -4 : 4;

	while (remsz > 0)
	{
		uint32_t cur_addr = addr & 0x1ffffc;

		uint32_t src_word = (remsz == 1) ? 0xffffff : (addr - 4) & 0x1ffffc;
		bus->write<uint32_t>(cur_addr, src_word);
		
		addr += inc;
		remsz -= 1;
	}

	chan.chcr.busy = chan.chcr.start = 0;
}

void DMA::HandleGPU()
{
	auto& chan = channels[2];
	
	int inc = chan.chcr.address_step ? -4 : 4;

	//printf("[emu/DMA]: Doing transfer on channel 2\n");

	if (chan.chcr.sync_mode == 2)
	{
		uint32_t addr = chan.madr & 0x1ffffc;

		for (;;)
		{
			uint32_t header = bus->read<uint32_t>(addr);

			int remsz = header >> 24;

			while (remsz > 0)
			{
				addr = (addr + 4) & 0x1ffffc;

				uint32_t command = bus->read<uint32_t>(addr);

				bus->write<uint32_t>(0x1f801810, command);

				remsz--;
			}

			if ((header & 0x800000) != 0)
				break;
			
			addr = header & 0x1ffffc;
		}
		chan.chcr.busy = chan.chcr.start = 0;
	}
	else if (chan.chcr.sync_mode == 1 || chan.chcr.sync_mode == 0)
	{
		int remsz = chan.bcr.blocksize * chan.bcr.block_count;
		uint32_t addr = chan.madr;

		while (remsz > 0)
		{
			uint32_t cur_addr = addr & 0x1ffffc;

			uint32_t src_word = bus->read<uint32_t>(cur_addr);

			bus->write<uint32_t>(0x1f801810, src_word);

			addr += inc;
			remsz -= 1;
		}
		chan.chcr.busy = chan.chcr.start = 0;
	}
	else
	{
		printf("Unhandled DMA mode %d on channel 2\n", chan.chcr.sync_mode);
		exit(1);
	}
}

uint32_t DMA::read_dma(uint32_t addr)
{
	int channel = ((addr >> 4) & 0xf) - 0x8;
	int reg = addr & 0xf;

	switch (reg)
	{
	case 0x0:
		return channels[channel].madr;
	case 0x4:
		return channels[channel].bcr.value;
	case 0x8:
		return channels[channel].chcr.value;
	default:
		printf("[emu/DMA]: Read from unknown address 0x%08x\n", addr);
		exit(1);
	}
}

#pragma once

#include <cstdint>
#include <functional>

class Bus;

class DMA
{
private:
	uint32_t dpcr;
	uint32_t dicr;

	union DN_CHCR
	{
		uint32_t value;
		struct
		{
			uint32_t direction : 1,
			address_step : 1,
			: 6,
			chopping : 1,
			sync_mode : 2,
			: 5,
			dma_window_size : 3,
			: 1,
			cpu_window_size : 3,
			: 1,
			busy : 1,
			: 3,
			start : 1,
			end : 3;
		};
	};

	union BCR
	{
		uint32_t value;
		struct
		{
			uint32_t word_count : 16;
			uint32_t : 16;
		};
		struct
		{
			uint32_t blocksize : 16;
			uint32_t block_count : 16;
		};
	};

	struct Channel
	{
		uint32_t madr;
		DN_CHCR chcr;
		BCR bcr;
		std::function<void()> RunFunc;
	} channels[7];

	Bus* bus;

	void HandleOTC();
	void HandleGPU();
public:
	DMA(Bus* bus);

	uint32_t read(uint32_t addr);
	void write(uint32_t addr, uint32_t data);

	void write_dma(uint32_t addr, uint32_t data);
	uint32_t read_dma(uint32_t addr);
};
#include "Bus.h"
#include <fstream>
#include <cstring>
#include <emu/cpu/cpu.h>

Bus::Bus(std::string biosFileName)
{
	std::ifstream file(biosFileName, std::ios::ate | std::ios::binary);
	size_t size = file.tellg();
	file.seekg(0, std::ios::beg);

	file.read((char*)bios, size);
	file.close();

	dma = new DMA(this);
	gpu = new GPU();
	dvd = new CDVD(this);
	timers = new Timers(this);
}

void Bus::Dump()
{
	std::ofstream out("mem.bin");
	
	for (int i = 0; i < 0x200000; i++)
		out << ram[i];
	
	out.close();
}

void Bus::TriggerInterrupt(int interrupt)
{
	I_STAT |= (1 << interrupt);
}

struct PSEXEHeader
{
	char magic[8];
	char zero[8];
	uint32_t initial_pc;
	uint32_t initial_gp;
	uint32_t destination;
	uint32_t file_size;
	uint32_t data_section_start;
	uint32_t data_section_size;
	uint32_t bss_section_start;
	uint32_t bss_section_size;
	uint32_t initial_sp_base;
	uint32_t initial_sp_offset;
	char reserved[20];
	char ascii_marker[];
};

void Bus::LoadEXE(std::string exe_name, CPU* cpu)
{
	std::ifstream file(exe_name, std::ios::ate | std::ios::binary);
	size_t size = file.tellg();
	uint8_t* buf = new uint8_t[size];
	file.seekg(0, std::ios::beg);
	file.read((char*)buf, size);

	file.close();

	PSEXEHeader* hdr = (PSEXEHeader*)buf;

	if (std::strncmp(hdr->magic, "PS-X EXE", 8))
	{
		printf("Invalid PS-X EXE magic\n");
		exit(1);
	}

	printf("[emu/Bus]: Loading EXE to 0x%08x\n", hdr->destination);
	printf("[emu/Bus]: Stack is at 0x%08x\n", hdr->initial_sp_base + hdr->initial_sp_offset);\

	for (int i = 0; i < hdr->file_size; i++)
	{
		write<uint8_t>(hdr->destination+i, buf[i+0x800]);
	}

	cpu->JumpToExe(hdr->initial_pc, hdr->initial_sp_base + hdr->initial_sp_offset);

	delete[] buf;
}

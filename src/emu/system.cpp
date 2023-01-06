#include "system.h"
#include <emu/cpu/cpu.h>
#include <emu/memory/Bus.h>
#include <emu/renderer/renderer.h>

CPU* cpu;
Bus* bus;
renderer::Renderer* g_renderer;

System::System(std::string biosPath)
{
	bus = new Bus(biosPath);
	cpu = new CPU(bus);
	g_renderer = new renderer::Renderer();
	g_renderer->gpu = bus->get_gpu();
	g_renderer->set_texture_size(VRAM_WIDTH, VRAM_HEIGHT);
}

void System::Clock()
{
	for (int i = 0; i < 564480 / 100; i += 2)
	{
		cpu->Clock(100);
		bus->step();
	}

	g_renderer->render((const void*)bus->get_gpu()->GetVram().data());
	bus->TriggerInterrupt(0);
}

void System::Dump()
{
	bus->Dump();
	cpu->Dump();
	bus->gpu->Dump();
}

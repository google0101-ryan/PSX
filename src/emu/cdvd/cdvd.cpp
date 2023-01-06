#include "cdvd.h"
#include <emu/memory/Bus.h>

void CDVD::write_reg2(uint8_t data)
{
	switch (cdrom_status.index)
	{
	case 0:
	{
		param_fifo.push_back(data);
		cdrom_status.param_fifo_empty = false;
		cdrom_status.param_fifo_write_ready = (param_fifo.size() < 16);
		break;
	}
	case 1:
		reg_int_enabled = data;
		break;
	default:
		printf("[emu/CDVD]: Write to unknown address 0x1f801802.%d\n", cdrom_status.index);
		exit(1);
	}
}

void CDVD::execute_command(uint8_t cmd)
{
	irq_fifo.clear();
	resp_fifo.clear();

	switch (cmd)
	{
	case 0x01:
		push_response(FirstInt3, {stat_code.byte});
		break;
	case 0x19:
	{
		const auto subfunc = param_fifo.front();
		param_fifo.pop_front();

		switch (subfunc)
		{
		case 0x20:
			push_response(FirstInt3, {0x94, 0x09, 0x19, 0xC0});
			break;
		default:
			printf("[emu/CDVD]: Unknown sub-command 0x19 0x%x\n", subfunc);
			exit(1);
		}
		break;
	}
	default:
		printf("[emu/CDVD]: Unknown command 0x%x\n", cmd);
		exit(1);
	}
}

void CDVD::push_response(CdromResponseType type, std::initializer_list<uint8_t> bytes)
{
	irq_fifo.push_back(type);

	for (auto response_byte : bytes)
	{
		if (resp_fifo.size() < 16)
		{
			resp_fifo.push_back(response_byte);
			cdrom_status.response_fifo_not_empty = true;
		}
	}
}

void CDVD::write_reg1(uint8_t data)
{
	switch (cdrom_status.index)
	{
	case 0:
		execute_command(data);
		break;
	case 1:
	case 2:
	case 3:
	{
		param_fifo.push_back(data);
		cdrom_status.param_fifo_empty = false;
		cdrom_status.param_fifo_write_ready = (param_fifo.size() < 16);
		break;
	}
	default:
		printf("[emu/CDVD]: Write to unknown address 0x1f801801.%d\n", cdrom_status.index);
		exit(1);
	}
}

void CDVD::write_reg3(uint8_t data)
{
	switch (cdrom_status.index)
	{
	case 1:
		if (data & 0x40)
		{
			param_fifo.clear();
			cdrom_status.param_fifo_empty = true;
			cdrom_status.param_fifo_write_ready = true;
		}

		if (!irq_fifo.empty())
			irq_fifo.pop_front();
		break;
	default:
		printf("[emu/CDVD]: Write to unknown address 0x1f801803.%d\n", cdrom_status.index);
		exit(1);
	}
}

uint8_t CDVD::read_reg3()
{
	switch (cdrom_status.index)
	{
	case 1:
	case 3:
	{
		uint8_t ret = 0b11100000;

		if (!irq_fifo.empty())
			ret |= irq_fifo.front() & 0b111;
		return ret;
	}
	default:
		printf("[emu/CDVD]: Read from unknown address 0x1f801803.%d\n", cdrom_status.index);
		exit(1);
	}
}

void CDVD::step()
{
	cdrom_status.transmit_busy = false;

	if (!irq_fifo.empty())
	{
		auto irq_triggered = irq_fifo.front() & 0b111;
		auto irq_mask = reg_int_enabled & 0b111;

		if (irq_triggered & irq_mask)
			bus->TriggerInterrupt(2);
	}
}

void CDVD::write(uint32_t addr, uint32_t data)
{
	switch (addr)
	{
	case 0x1f801800:
		cdrom_status.index = data & 0b11;
		break;
	case 0x1f801801:
		write_reg1(data);
		break;
	case 0x1f801802:
		write_reg2(data);
		break;
	case 0x1f801803:
		write_reg3(data);
		break;
	default:
		printf("[emu/CDVD]: Write to unknown address 0x%08x\n", addr);
		exit(1);
	}
}

uint32_t CDVD::read(uint32_t addr)
{
	switch (addr)
	{
	case 0x1f801800:
		return cdrom_status.byte;
	case 0x1f801801:
	{
		if (!resp_fifo.empty())
		{
			uint8_t ret = resp_fifo.front();
			resp_fifo.pop_front();

			if (resp_fifo.empty())
				cdrom_status.response_fifo_not_empty = false;
			return ret;
		}
		return 0;
	}
	case 0x1f801803:
		return read_reg3();
	default:
		printf("[emu/CDVD]: Read from unknown address 0x%08x\n", addr);
		exit(1);
	}
}

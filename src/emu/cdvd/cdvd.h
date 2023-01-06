#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>

class Bus;

enum CdromResponseType : uint8_t 
{
  NoneInt0 = 0,     // INT0: No response received (no interrupt request)
  SecondInt1 = 1,   // INT1: Received SECOND (or further) response to ReadS/ReadN (and Play+Report)
  SecondInt2 = 2,   // INT2: Received SECOND response (to various commands)
  FirstInt3 = 3,    // INT3: Received FIRST response (to any command)
  DataEndInt4 = 4,  // INT4: DataEnd (when Play/Forward reaches end of disk) (maybe also for Read?)
  ErrorInt5 = 5,    // INT5: Received error-code (in FIRST or SECOND response)
};

enum class CdromReadState 
{
  Stopped,
  Seeking,
  Playing,
  Reading,
};

union CdromStatusCode 
{
  uint8_t byte{};

  struct 
  {
    uint8_t error : 1;
    uint8_t spindle_motor_on : 1;
    uint8_t seek_error : 1;
    uint8_t id_error : 1;
    uint8_t shell_open : 1;
    uint8_t reading : 1;
    uint8_t seeking : 1;
    uint8_t playing : 1;
  };

  CdromStatusCode() { shell_open = true; }

  // Does not reset shell open state
  void reset() { error = spindle_motor_on = seek_error = id_error = reading = seeking = playing = 0; }

  void set_state(CdromReadState state) 
  {
    reset();
    spindle_motor_on = true;  // Turn on motor
    switch (state) {
      case CdromReadState::Seeking: seeking = true; break;
      case CdromReadState::Playing: playing = true; break;
      case CdromReadState::Reading: reading = true; break;
    }
  }
};

class CDVD
{
private:
	union CdromStatusRegister 
	{
		uint8_t byte{};

		struct 
		{
			uint8_t index : 2;
			uint8_t adpcm_fifo_empty : 1;         // set when playing XA-ADPCM sound
			uint8_t param_fifo_empty : 1;         // triggered before writing 1st byte
			uint8_t param_fifo_write_ready : 1;   // triggered after writing 16 bytes
			uint8_t response_fifo_not_empty : 1;  // triggered after reading LAST byte
			uint8_t data_fifo_not_empty : 1;      // triggered after reading LAST byte
			uint8_t transmit_busy : 1;            // Command/parameter transmission busy
		};

		CdromStatusRegister() 
		{
			param_fifo_empty = true;
			param_fifo_write_ready = true;
		}
	} cdrom_status;

	void execute_command(uint8_t cmd);

	void push_response(CdromResponseType type, std::initializer_list<uint8_t> bytes);

	void write_reg1(uint8_t data);
	void write_reg2(uint8_t data);
	void write_reg3(uint8_t data);

	uint8_t read_reg3();

	std::deque<uint8_t> param_fifo;
	std::deque<uint8_t> resp_fifo;
	std::deque<CdromResponseType> irq_fifo;

	CdromStatusCode stat_code;

	uint8_t reg_int_enabled;

	Bus* bus;
public:
	CDVD(Bus* bus) : bus(bus) {}

	void step();

	void write(uint32_t addr, uint32_t data);
	uint32_t read(uint32_t addr);
};
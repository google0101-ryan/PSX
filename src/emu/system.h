#pragma once

#include <string>

class System
{
public:
	System(std::string biosPath);

	void Clock();
	void Dump();
};
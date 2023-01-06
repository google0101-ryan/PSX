#pragma once

#include <cstdint>
#include <cstdio>

#include <emu/system.h>

extern System* _sys;

class Application
{
private:
    static bool initialized;
public:
    static bool Init(int, char**);

    static void Run();
    static void Exit(int code);
    static void Exit();
};
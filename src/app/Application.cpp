#include <app/Application.h>
#include <cstdlib>
#include <csignal>

bool Application::initialized = false;
System* _sys = nullptr;

void sig_handler(int)
{
	printf("Sigint handler\n");
	exit(1);
}

bool Application::Init(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("Usage: %s [bios]\n", argv[0]);
		exit(1);
	}

    _sys = new System(argv[1]);

    std::atexit(Exit);

    if (!_sys)
    {
        printf("ERR: Could not initialize system\n");
        return false;
    }

	std::signal(SIGINT, sig_handler);

    return true;
}

void Application::Run()
{
    while (1)
        _sys->Clock();
}

void Application::Exit()
{
    _sys->Dump();
    printf("Exiting\n");
}

void Application::Exit(int code)
{
    exit(code);
}
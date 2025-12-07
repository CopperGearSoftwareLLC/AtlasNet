#include "Launcher.hpp"
#include "Debug/Crash/CrashHandler.hpp"

int main(int argc, char **argv)
{
    CrashHandler::Get().Init();
    Launcher launcher;
    launcher.Run();
}
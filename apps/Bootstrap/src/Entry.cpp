#include "Bootstrap.hpp"
#include "Debug/Crash/CrashHandler.hpp"

int main(int argc, char **argv)
{
    CrashHandler::Get().Init();
    Bootstrap bs;
    bs.Run();
}
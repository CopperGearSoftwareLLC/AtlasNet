#include "Cartograph.hpp"
#include <Debug/Crash/CrashHandler.hpp>
int main(int argc, char** argv)
{
    CrashHandler::Get().Init();

	Cartograph::Get().Run();
	
}
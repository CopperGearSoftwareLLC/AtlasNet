#include <chrono>
#include <thread>

#include "Proxy.hpp"
#include "pch.hpp"

int main()
{
	Proxy::Get().Run();

	return 0;
}
#include <chrono>
#include <thread>

#include "Proxy.hpp"
#include "Global/pch.hpp"

int main()
{
	Proxy::Get().Run();

	return 0;
}
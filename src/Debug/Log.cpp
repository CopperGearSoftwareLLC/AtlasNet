#include "Log.hpp"

Log::Log(const std::string &Who) : WhoIsTalking(Who)
{
	std::hash<std::string> hasher;
	size_t hashValue = hasher(WhoIsTalking);

	size_t colorIndex = hashValue % Logcolors.size();
	Color = Logcolors[colorIndex];
}




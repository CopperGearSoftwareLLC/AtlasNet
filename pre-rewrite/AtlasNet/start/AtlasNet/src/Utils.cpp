#include "Utils.hpp"

#include <cstdlib>
#include <stdexcept>

std::filesystem::path GetAtlasNetPath()
{
	if (const char* ATLASNET_ROOT_ENV = std::getenv("ATLASNET_ROOT"); ATLASNET_ROOT_ENV)
	{
		return ATLASNET_ROOT_ENV;
	}
	else
	{
		throw std::runtime_error("ATLASNET_ROOT must be defined");
	}
}

std::filesystem::path GetSDKPath()
{
	return GetAtlasNetPath() / "sdk";  // relative to bin/
}

std::filesystem::path GetEXEPath()
{
	// Get the directory of the executable
	std::filesystem::path exe_dir =
		std::filesystem::read_symlink("/proc/self/exe").parent_path();	// Linux
	// On Windows, use GetModuleFileName()
	return exe_dir;	 // relative to bin/
}

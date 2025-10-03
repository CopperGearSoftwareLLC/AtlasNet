

#pragma once
#include "pch.hpp"
class Log
{
  public:
	Log() = default;
	Log(const std::string &Who);

	std::string WhoIsTalking;

	template <typename... Args> void PrintFormatted(std::string_view fmt, Args &&...args) const
	{
		std::string message = std::vformat(fmt, std::make_format_args(args...));
		Print(message);
	}
	void Print(std::string_view str) const
	{
		std::cerr << Color << WhoIsTalking << "> " << resetColor << str << std::endl;
	}
	

  private:
	std::string Color;
	const std::string resetColor = "\033[0m";
	// Expanded array of ANSI color codes (foreground colors)
	const std::vector<std::string> Logcolors = {
		"\033[30m", // Black
		"\033[31m", // Red
		"\033[32m", // Green
		"\033[33m", // Yellow
		"\033[34m", // Blue
		"\033[35m", // Magenta
		"\033[36m", // Cyan
		"\033[90m", // Bright Black (Gray)
		"\033[91m", // Bright Red
		"\033[92m", // Bright Green
		"\033[93m", // Bright Yellow
		"\033[94m", // Bright Blue
		"\033[95m", // Bright Magenta
		"\033[96m", // Bright Cyan

	};

	
};

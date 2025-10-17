#include "Log.hpp"

Log::Log(const std::string &Who) : WhoIsTalking(Who)
{
	IdentifierColor = GetRandomTerminalColor();
}

std::string Log::GetTerminalColorCode(TerminalColor color, bool foreground) 
{
	int base = foreground ? 30 : 40;		// Foreground: 30–37, Background: 40–47
	int brightBase = foreground ? 90 : 100; // Bright Foreground: 90–97, Bright Background: 100–107

	switch (color)
	{
	case TerminalColor::Black:
		return "\033[" + std::to_string(base + 0) + "m";
	case TerminalColor::Red:
		return "\033[" + std::to_string(base + 1) + "m";
	case TerminalColor::Green:
		return "\033[" + std::to_string(base + 2) + "m";
	case TerminalColor::Yellow:
		return "\033[" + std::to_string(base + 3) + "m";
	case TerminalColor::Blue:
		return "\033[" + std::to_string(base + 4) + "m";
	case TerminalColor::Magenta:
		return "\033[" + std::to_string(base + 5) + "m";
	case TerminalColor::Cyan:
		return "\033[" + std::to_string(base + 6) + "m";
	case TerminalColor::White:
		return "\033[" + std::to_string(base + 7) + "m";
	case TerminalColor::BrightBlack:
		return "\033[" + std::to_string(brightBase + 0) + "m";
	case TerminalColor::BrightRed:
		return "\033[" + std::to_string(brightBase + 1) + "m";
	case TerminalColor::BrightGreen:
		return "\033[" + std::to_string(brightBase + 2) + "m";
	case TerminalColor::BrightYellow:
		return "\033[" + std::to_string(brightBase + 3) + "m";
	case TerminalColor::BrightBlue:
		return "\033[" + std::to_string(brightBase + 4) + "m";
	case TerminalColor::BrightMagenta:
		return "\033[" + std::to_string(brightBase + 5) + "m";
	case TerminalColor::BrightCyan:
		return "\033[" + std::to_string(brightBase + 6) + "m";
	case TerminalColor::BrightWhite:
		return "\033[" + std::to_string(brightBase + 7) + "m";
	default:
		return "\033[0m"; // Reset
	}
}

Log::TerminalColor Log::GetRandomTerminalColor() 
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<int> dist(0, static_cast<int>(TerminalColor::BrightWhite));

	return static_cast<TerminalColor>(dist(gen));
}


#pragma once
#include "pch.hpp"
class Log
{
public:
	Log() = default;
	Log(const std::string &Who);

	std::string WhoIsTalking;

	template <typename... Args>
	void ErrorFormatted(std::string_view fmt, Args &&...args) const
	{
		std::string message = std::vformat(fmt, std::make_format_args(args...));
		Error(message);
	}
	void Error(std::string_view str) const
	{
		std::cerr << GetTerminalColorCode(IdentifierColor, true) << WhoIsTalking << "> " << ResetColor() << GetTerminalColorCode(TerminalColor::Red) << str << ResetColor() << std::endl;
	}
	template <typename... Args>
	void WarningFormatted(std::string_view fmt, Args &&...args) const
	{
		std::string message = std::vformat(fmt, std::make_format_args(args...));
		Warning(message);
	}
	void Warning(std::string_view str) const
	{
		std::cerr << GetTerminalColorCode(IdentifierColor, true) << WhoIsTalking << "> " << ResetColor() << GetTerminalColorCode(TerminalColor::Yellow) << str << ResetColor() << std::endl;
	}
	template <typename... Args>
	void DebugFormatted(std::string_view fmt, Args &&...args) const
	{
		std::string message = std::vformat(fmt, std::make_format_args(args...));
		Debug(message);
	}
	void Debug(std::string_view str) const
	{
		std::cerr << GetTerminalColorCode(IdentifierColor, true) << WhoIsTalking << "> " << ResetColor() << str << ResetColor() << std::endl;
	}

	enum class TerminalColor
	{
		Black,
		Red,
		Green,
		Yellow,
		Blue,
		Magenta,
		Cyan,
		White,
		BrightBlack,
		BrightRed,
		BrightGreen,
		BrightYellow,
		BrightBlue,
		BrightMagenta,
		BrightCyan,
		BrightWhite,
	};
	static std::string GetTerminalColorCode(TerminalColor color, bool foreground = true);

	static TerminalColor GetRandomTerminalColor();

private:
	TerminalColor IdentifierColor;

	static inline std::string ResetColor() { return "\033[0m"; }
};

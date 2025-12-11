

#pragma once
#include "pch.hpp"
class Log
{
public:
	Log() = default;
    Log(const std::string &Who);

    enum class Level
    {
        Error = 0,
        Warning = 1,
        Debug = 2,
    };

	std::string WhoIsTalking;

	template <typename... Args>
	void ErrorFormatted(std::string_view fmt, Args &&...args) const
	{
		std::string message = std::vformat(fmt, std::make_format_args(args...));
		Error(message);
	}
	void Error(std::string_view str) const
	{
        if (IsMuted())
            return;
        // If a UI sink is installed (e.g., FTXUI), route logs there to avoid corrupting the UI.
        if (HasUISink())
        {
            EmitToUISink(std::vformat("{} | {}", std::make_format_args(WhoIsTalking, str)));
            return;
        }
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
        if (IsMuted())
            return;
        if (GetLevel() < Level::Warning)
            return;
        if (HasUISink())
        {
            EmitToUISink(std::vformat("{} | {}", std::make_format_args(WhoIsTalking, str)));
            return;
        }
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
        if (IsMuted())
            return;
        if (GetLevel() < Level::Debug)
            return;
        if (HasUISink())
        {
            EmitToUISink(std::vformat("{} | {}", std::make_format_args(WhoIsTalking, str)));
            return;
        }
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

    // --- Optional UI sink (e.g., FTXUI) ---
    // When installed, Log will not write to std::cerr; instead it will forward plain text lines to the sink.
    using UISink = std::function<void(const std::string&)>;
    static void SetUISink(UISink sink)
    {
        std::scoped_lock lock(ui_sink_mutex);
        ui_sink = std::move(sink);
    }
    static bool HasUISink()
    {
        std::scoped_lock lock(ui_sink_mutex);
        return static_cast<bool>(ui_sink);
    }
    static void SetMuted(bool value)
    {
        muted.store(value, std::memory_order_relaxed);
    }
    static bool IsMuted()
    {
        return muted.load(std::memory_order_relaxed);
    }
    static void SetLevel(Level new_level)
    {
        log_level.store(static_cast<int>(new_level), std::memory_order_relaxed);
    }
    static Level GetLevel()
    {
        return static_cast<Level>(log_level.load(std::memory_order_relaxed));
    }
    static void InitFromEnv()
    {
        const char *env = std::getenv("ATLAS_LOG_LEVEL");
        if (!env) return;
        std::string v = env;
        for (auto &ch : v) ch = std::toupper(static_cast<unsigned char>(ch));
        if (v == "ERROR") SetLevel(Level::Error);
        else if (v == "WARNING" || v == "WARN") SetLevel(Level::Warning);
        else if (v == "DEBUG") SetLevel(Level::Debug);
    }

private:
	TerminalColor IdentifierColor;
    inline static UISink ui_sink{};
    inline static std::mutex ui_sink_mutex;
    inline static std::atomic_bool muted{false};
    inline static std::atomic<int> log_level{static_cast<int>(Level::Debug)};
    static void EmitToUISink(const std::string &line)
    {
        std::scoped_lock lock(ui_sink_mutex);
        if (ui_sink)
            ui_sink(line);
    }

	static inline std::string ResetColor() { return "\033[0m"; }
};

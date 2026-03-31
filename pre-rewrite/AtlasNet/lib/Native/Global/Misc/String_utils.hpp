#pragma once
#include <filesystem>
#include <string>
#include <cctype>
#include <regex>
#include <fstream>
static std::string NukeString(const std::string& input)
{
    std::string out;
    out.reserve(input.size());

    // Copy only valid, visible characters
    for (unsigned char c : input)
    {
        // Remove nulls and control characters except standard printable ones
        if (c == '\0') continue;
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') continue;

        out.push_back(c);
    }

    // Now trim leading/trailing whitespace/newlines/tabs
    size_t start = 0;
    while (start < out.size() && (std::isspace((unsigned char)out[start]) || out[start] == '\0'))
        ++start;

    size_t end = out.size();
    while (end > start && (std::isspace((unsigned char)out[end - 1]) || out[end - 1] == '\0'))
        --end;

    return out.substr(start, end - start);
}

static std::string ToLower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}
static std::string MacroParse(const std::string& input,
                              const std::unordered_map<std::string, std::string>& macros)
{
    static const std::regex macroPattern(R"(\$\{([^}]+)\})");

    std::string result;
    result.reserve(input.size());

    std::sregex_iterator it(input.begin(), input.end(), macroPattern);
    std::sregex_iterator end;

    size_t lastPos = 0;

    // First-pass substitution
    for (; it != end; ++it)
    {
        const std::smatch& match = *it;

        // Append raw text before match
        result.append(input, lastPos, match.position() - lastPos);

        std::string key = match[1].str(); // extract VAR in ${VAR}

        if (auto found = macros.find(key); found != macros.end())
        {
            result += found->second; // substitute
        }
        else
        {
            result += match.str(); // keep original ${VAR}
        }

        lastPos = match.position() + match.length();
    }

    // Append any leftover text after last match
    result.append(input, lastPos, std::string::npos);

    // --- Check for unresolved macros that CAN be expanded ---
    bool needsReparse = false;

    std::sregex_iterator checkIt(result.begin(), result.end(), macroPattern);
    for (; checkIt != end; ++checkIt)
    {
        std::string key = (*checkIt)[1].str();
        if (macros.contains(key)) {
            needsReparse = true;
            break;
        }
    }

    // Re-run only if we discovered *expandable* macros
    if (needsReparse)
        return MacroParse(result, macros);

    return result;
}
inline void WriteTextFile(const std::string& path, const std::string& content)
{

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream out(path);
    if (!out.is_open())
        throw std::runtime_error("Failed to open file: " + path);

    out << content;
}
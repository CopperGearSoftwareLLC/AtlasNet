#pragma once
#include <string>
#include <cctype>

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
static std::string MacroParse(std::string input, std::unordered_map<std::string, std::string> macros)
{
    std::regex macroPattern(R"(\$\{([^}]+)\})");
    std::string result;
    std::sregex_iterator it(input.begin(), input.end(), macroPattern);
    std::sregex_iterator end;

    size_t lastPos = 0;
    result.reserve(input.size());

    while (it != end)
    {

        for (; it != end; ++it)
        {
            const std::smatch &match = *it;
            result.append(input, lastPos, match.position() - lastPos);

            std::string key = match[1].str();
            auto found = macros.find(key);
            if (found != macros.end())
            {
                result += found->second;
            }

            else
                result += match.str(); // keep original ${VAR} if not found

            lastPos = match.position() + match.length();
        }
    }

    result.append(input, lastPos, std::string::npos);
    bool fullyParsed = true;
    if (auto it = std::sregex_iterator(result.begin(), result.end(), macroPattern); it != end)
    {
        for (; it != end; ++it)
            if (macros.find(it->str()) == macros.end())
            {
                fullyParsed = false;
            }
        if (fullyParsed)
            result = MacroParse(result, macros);
    };
    return result;
}
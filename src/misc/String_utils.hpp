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

#include "CrashHandler.hpp"
#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
  #include <vector>
#elif defined(__linux__)
  #include <unistd.h>
  #include <vector>
#endif
std::filesystem::path CrashHandler::get_executable_path()
{
    #if defined(_WIN32)
    std::wstring buf;
    buf.resize(260); // start with MAX_PATH-ish
    for (;;)
    {
        DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) throw std::runtime_error("GetModuleFileNameW failed");
        if (len < buf.size())
        {
            buf.resize(len);
            return std::filesystem::path(buf);
        }
        buf.resize(buf.size() * 2);
    }

#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size);
    if (_NSGetExecutablePath(buf.data(), &size) != 0)
        throw std::runtime_error("_NSGetExecutablePath failed");

    // returns a path that may contain symlinks/..; canonicalize if you want
    return std::filesystem::weakly_canonical(std::filesystem::path(buf.data()));

#elif defined(__linux__)
    std::vector<char> buf(1024);
    for (;;)
    {
        const ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size());
        if (n < 0) throw std::runtime_error("readlink(/proc/self/exe) failed");
        if (static_cast<size_t>(n) < buf.size())
            return std::filesystem::path(std::string(buf.data(), static_cast<size_t>(n)));
        buf.resize(buf.size() * 2);
    }

#else
    #error "get_executable_path not implemented for this platform"
#endif
}

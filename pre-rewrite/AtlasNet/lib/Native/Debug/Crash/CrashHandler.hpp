#pragma once
#include <boost/stacktrace/stacktrace.hpp>
#include <filesystem>
#include "Global/Misc/Singleton.hpp"
#include <csignal>
#include <execinfo.h>
#include <iostream>
class CrashHandler : public Singleton<CrashHandler>
{
    std::filesystem::path get_executable_path();
public:
    void Init()
    {
        programName = get_executable_path();
        auto HandleLambda = [](int sig)
        { CrashHandler::Get().HandleSignal(sig); };
        std::signal(SIGSEGV, HandleLambda);
        std::signal(SIGABRT, HandleLambda);
        std::signal(SIGFPE, HandleLambda);
        std::signal(SIGILL, HandleLambda);
    }

private:
    std::string programName;

    void HandleSignal(int sig)
    {
        void *array[32];
        size_t size = backtrace(array, 32);

        std::cerr << "\n\n*** Crash detected (signal " << sig << ") ***\n";
        std::cerr << boost::stacktrace::stacktrace();
        /*
        for (size_t i = 0; i < size; ++i)
        {
            std::stringstream cmd;
            cmd << "addr2line -f -p -e " << programName << " " << array[i];
            system(cmd.str().c_str()); // prints "func at file:line"
        }*/

        _exit(1); // exit immediately
    }
};
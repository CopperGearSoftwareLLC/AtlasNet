#pragma once
#include <pch.hpp>
#include "Singleton.hpp"

class CrashHandler : public Singleton<CrashHandler>
{
public:
    void Init(const std::string &binaryName)
    {
        programName = binaryName;
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
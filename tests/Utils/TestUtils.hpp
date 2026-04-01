#pragma once
#include "atlasnet/core/System.hpp"
#include <csignal>
#include <execinfo.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <boost/stacktrace.hpp>
class TestUtils : public AtlasNet::System<TestUtils>
{
public:
  TestUtils()
  {
    programName = get_executable_path();
    auto HandleLambda = [](int sig) { TestUtils::Get().HandleSignal(sig); };
    std::signal(SIGSEGV, HandleLambda);
    std::signal(SIGABRT, HandleLambda);
    std::signal(SIGFPE, HandleLambda);
    std::signal(SIGILL, HandleLambda);
  }
  std::filesystem::path get_executable_path();

private:
  std::string programName;

  void HandleSignal(int sig)
  {
    void* array[32];
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

  void Shutdown() override {}
};



static std::string PrefixLines(const std::string& text, const std::string& prefix)
{
  std::stringstream in(text);
  std::string line;
  std::string out;

  while (std::getline(in, line))
  {
    out += std::format("[{}] {}\n", prefix, line);
  }

  // Preserve case where input ended without newline but had content
  if (!text.empty() && text.back() != '\n' && !out.empty())
  {
    out.pop_back();
  }

  return out;
}
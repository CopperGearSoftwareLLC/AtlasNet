#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include "Bootstrap.hpp"
#include <iostream>

int main(int argc, char **argv)
{
    std::span<char*> args(argv, argc);
    //CrashHandler::Get().Init();
    for (int i = 0; i < argc;i++)
    {
        std::cout << argv[i] << "";
    }
    std::cerr << "\n";
    Bootstrap bs;
    const auto arg = std::find_if(args.begin(),args.end(),[](char * arg){return std::string(BOOTSTRAP_SETTINGS_FILE_FLAG) == arg;});

    Bootstrap::RunArgs Runargs;
    if (arg != args.end()) 
    {
        std::cerr << "Found settings file path flag\n";
        std::string SettingsPath = *std::next(arg);
        std::cerr << SettingsPath << std::endl;

        Runargs.AtlasNetSettingsPath = SettingsPath;
    }
    bs.Run(Runargs);
}
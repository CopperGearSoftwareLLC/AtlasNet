-- premake5.lua

workspace "GuacNet"
    architecture "x86_64"
    configurations { "DebugDocker","DebugLocal", "Release" }
    startproject "God"
    includedirs {"src"}
    pchheader "src/pch.hpp"
    pchsource "src/pch.cpp"

    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"
    includedirs{"lib/GNS/include","lib/glm"}
    links{"GameNetworkingSockets_s"}
    libdirs{"lib/GNS/lib"}
    filter "configurations:DebugDocker"
        symbols "On"
        defines {"_DOCKER"}

    filter "configurations:DebugLocal"
        defines {"_LOCAL","_DISPLAY"}
        files{"lib/imgui/*.cpp"}
        includedirs{"lib/imgui"}
        files{"lib/imgui/backends/*.cpp"}
        symbols "On"        
        includedirs{"lib/glfw/include","lib/glew/include"}
        links{"GLEW","glfw3","GL","X11"}
        libdirs{"lib/glfw/lib","lib/glew/lib"}
    filter "configurations:Release"
        defines {"_DOCKER"}
        optimize "On"

project "God"
    dependson {"KDNet"}
        links{"KDNet"}
    kind "ConsoleApp"
    language "C++"
    files { "src_runners/GodRun.cpp" }

project "Partition"
    dependson "KDNet"
    kind "ConsoleApp"
    language "C++"
    files { "src_runners/PartitionRun.cpp" }
    links{"KDNet"}

project "KDNet"
    kind "StaticLib"
    language "C++"
    files { "src/**.hpp","src/**.cpp" }

function customClean()
    -- Specify the directories or files to be cleaned
    local dirsToRemove = {
        "bin",
        "obj",
        "Intermediate",
        ".cache"
    }

    local filesToRemove = {
        "Makefile",
        "KDNet.make",
        "Partition.make",
        "God.make",
        "imgui.ini",
        "compile_commands.json"
    }

    -- Remove specified directories
    for _, dir in ipairs(dirsToRemove) do
        if os.isdir(dir) then
            os.rmdir(dir)
            print("Removed directory: " .. dir)
        end
    end

    -- Remove specified files
    for _, file in ipairs(filesToRemove) do
        if os.isfile(file) then
            os.remove(file)
            print("Removed file: " .. file)
        end
    end
end

-- Add the custom clean function to the clean action
newaction {
    trigger = "clean",
    description = "Custom clean action",
    execute = function()
        customClean()
    end
}
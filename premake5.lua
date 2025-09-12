-- premake5.lua

workspace "GuacNet"
    architecture "x86_64"
    configurations { "DebugDocker","DebugLocal", "Release" }
    startproject "God"
    includedirs {"src"}
    pchheader "src/pch.hpp"
    pchsource "src/pch.cpp"
    location "build"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"
    includedirs{"lib/GameNetworkingSockets/include","lib/glm"}
    links{"GameNetworkingSockets_s","protobuf","crypto","ssl","curl"}
    libdirs{"lib/GameNetworkingSockets/lib"}
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

    project "Interlink"
        kind "StaticLib"
        language "C++"
        files {"src/Interlink/**.cpp"}
    project "KDNet"
        kind "StaticLib"
        dependson "Interlink"
        links "Interlink"
        language "C++"
        files { "src/KDNet/**.cpp" }

    project "God"
        dependson "Interlink"
        links {"Interlink"}
        kind "ConsoleApp"
        language "C++"
        files { "src/God/**.cpp" }
        defines "_GOD"
   project "GodView"
        dependson {"God","Interlink"}
        links {"God","Interlink"}
        kind "ConsoleApp"
        language "C++"
        files { "src/GodView/**.cpp" }
        defines "_GODVIEW"
    project "Partition"
        dependson "Interlink"
        links "Interlink"
        kind "ConsoleApp"
        language "C++"
        files { "src/Partition/**.cpp" }
        defines "_PARTITION"

    project "SampleGame"
        kind "ConsoleApp"
        dependson {"KDNet","Interlink"}
        links {"KDNet","Interlink"}
        language "C++"
        files { "src/SampleGame/**.cpp" }
        defines {"_GAMECLIENT","_GAMESERVER"}
function customClean()
    -- Specify the directories or files to be cleaned
    local dirsToRemove = {
        "bin",
        "obj",
        "Intermediate",
        ".cache",
        "build",
        "docker"
        
    }

    local filesToRemove = {
        "Makefile",
        "imgui.ini",
        "compile_commands.json"
    }

    local extensionsToRemove = {
--        ".make",
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
        local rootFiles = os.matchfiles("*") -- only root files
    for _, file in ipairs(rootFiles) do
        for _, ext in ipairs(extensionsToRemove) do
            if file:sub(-#ext) == ext then
                os.remove(file)
                print("Removed file by extension: " .. file)
            end
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
-- premake5.lua
local _PORT_GOD = 25564
local _PORT_PARTITION = 25565
local _PORT_GAMESERVER = 25566
local _PORT_GAMECOORDINATOR = 25567
local _PORT_DEMIGOD = 25568
local DevPackages = "git tini gdbserver build-essential binutils automake libtool m4 autoconf gdb curl zip less unzip ccache coreutils tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev"
workspace "GuacNet"
    --architecture "x86_64"
    configurations { "DebugDocker", "DebugLocal", "Release" }
    startproject "God"
    includedirs { "src" }

    location "build"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"

    filter "system:windows"
        includedirs { "vcpkg_installed/x64-windows/include" }
        libdirs { "vcpkg_installed/x64-windows/lib" }

    filter "system:linux"
        includedirs { "vcpkg_installed/x64-linux/include" }
        libdirs { "vcpkg_installed/x64-linux/lib" }

    filter {}     -- clear filter so it doesn’t leak into other settings
    
    links { "boost_stacktrace_addr2line","boost_container", "curl", "GameNetworkingSockets", "GLEW", "glfw3", "glm", "imgui","implot","implot3d","GL","curl","ssl","crypto","z","dl","redis++","hiredis" }
    defines {"BOOST_STACKTRACE_LINK","BOOST_STACKTRACE_USE_ADDR2LINE"}


    defines {"_PORT_GOD=".._PORT_GOD,
    "_PORT_PARTITION=".._PORT_PARTITION,
    "_PORT_GAMESERVER=".._PORT_GAMESERVER,
    "_PORT_GAMECOORDINATOR=".._PORT_GAMECOORDINATOR,
    "_PORT_DEMIGOD=".._PORT_DEMIGOD}

    filter "configurations:DebugDocker"
        symbols "On"
        defines { "_DOCKER" }

    filter "configurations:DebugLocal"
        defines { "_LOCAL",
        "_DISPLAY" }
        files { "lib/imgui/**.cpp" }
        symbols "On"
        includedirs { "lib/glfw/include", "lib/glew/include" }
        defines { "GLEW_STATIC" }
        links { "GLEW", "glfw3", "GL", "X11" }
        libdirs { "lib/glfw/lib", "lib/glew/lib" }
    filter "configurations:Release"
        defines { "_DOCKER" }
        symbols "On"
        optimize "On"
    project "AtlasNetLib"
        kind "StaticLib"
        language "C++"
        files { "src/**.cpp" }
        pchheader "src/pch.hpp"
        pchsource "src/pch.cpp"
    project "AtlasNet"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/AtlasNetRun.cpp" }
        pchheader "src/pch.hpp"
        pchsource "src/pch.cpp"
    project "God"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/GodRun.cpp" }
        defines "_GOD"
    project "Cartograph"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/CartographRun.cpp" }
        defines "_CARTOGRAPH"
    project "Partition"
        dependson "AtlasNetLib"
        links {"AtlasNetLib",}
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/PartitionRun.cpp" }
        defines "_PARTITION"
    project "Database"
        dependson "AtlasNetLib"
        links {"AtlasNetLib"}
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/DatabaseRun.cpp" }
        defines "_PARTITION"
    project "UnitTestsServer"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "Tests/SampleGame/Server/**.cpp" }
    project "UnitTestsClient"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "Tests/SampleGame/Client/**.cpp" }
    project "AtlasUnityBridge"
        kind "SharedLib"
        staticruntime "off"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
    project "AtlasClientBridge"
        kind "SharedLib"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
    project "GameCoordinator"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/GameCoordinatorRun.cpp" }
    project "Demigod"
        dependson "AtlasNetLib"
        links "AtlasNetLib"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/DemigodRun.cpp" }


-- Generic cleanup function
function customClean(dirsToRemove, filesToRemove)
    -- Remove specified directories
    for _, dir in ipairs(dirsToRemove or {}) do
        if os.isdir(dir) then
            os.rmdir(dir)
            os.execute('rm -rf "' .. dir .. '"')
            print("Removed directory: " .. dir)
        end
    end

    -- Remove specified files
    for _, file in ipairs(filesToRemove or {}) do
        if os.isfile(file) then
            os.remove(file)
            print("Removed file: " .. file)
        end
    end
end

function CleanBin()
    local dirs = { "bin", "obj", "build", "docker" };
    local files = {}
    customClean(dirs, files)
end

function CleanDeps()
    local dirs = { "vcpkg", "vcpkg_installed" };
    local files = {}
    customClean(dirs, files)
end

function CleanDocs()
    local dirs = { "docs" };
    local files = {}
    customClean(dirs, files)
end

-- Add the custom clean function to the clean action
newaction {
    trigger = "CleanBin",
    description = "Custom clean action",
    execute = function()
        CleanBin()
    end
}
newaction {
    trigger = "CleanDeps",
    description = "Custom clean action",
    execute = function()
        CleanDeps()
    end
}
newaction {
    trigger = "CleanDocs",
    description = "Custom clean action",
    execute = function()
        CleanDocs()
    end
}
newaction {
    trigger = "CleanAll",
    description = "Custom clean action",
    execute = function()
        CleanBin()
        CleanDeps()
        CleanDocs()
    end
}
newaction
{
    trigger = "setup",
    description = "Setups up dependencies",
    execute = function()
        local isWindows = package.config:sub(1, 1) == '\\' -- check if path separator is '\'

        os.execute("git clone https://github.com/microsoft/vcpkg.git")

        if isWindows then
            os.execute("vcpkg\\bootstrap-vcpkg.bat")
            os.execute("vcpkg\\vcpkg install")
        else
            os.execute("./vcpkg/bootstrap-vcpkg.sh")
            os.execute("sudo apt-get update && sudo apt-get install "..DevPackages.." -y")
            os.execute("./vcpkg/vcpkg install")
        end
    end

}
newaction
{
    trigger = "GenDocs",
    description = "generate documentation",
    execute = function()
        os.execute("doxygen Doxyfile")
    end

}
newaction {
    trigger = "AtlasNetStart",
    description = "build",
    execute = function()

      os.execute("./premake5 gmake")
      os.execute("make -C build -j $(nproc)")
      os.execute("./bin/DebugDocker/AtlasNet/AtlasNet")
    end
}

-- premake5.lua

workspace "GuacNet"
    architecture "x86_64"
    configurations { "DebugDocker","DebugLocal", "Release" }
    startproject "God"
    includedirs {"src"}

    location "build"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"
    --includedirs{"lib/GameNetworkingSockets/include","lib/glm","lib/json/single_include","lib"}
    --links{"GameNetworkingSockets_s","protobuf","crypto","ssl","curl"}
    --libdirs{"lib/GameNetworkingSockets/lib"}
    includedirs{"vcpkg_installed/x64-linux/include"}
    libdirs "vcpkg_installed/x64-linux/lib"
    links{"boost_container","curl","GameNetworkingSockets","GLEW","glfw3","glm","imgui"}
    
    filter "configurations:DebugDocker"
        symbols "On"
        defines {"_DOCKER"}

    filter "configurations:DebugLocal"
        defines {"_LOCAL",
        "_DISPLAY"}
        files { "lib/imgui/**.cpp" }
        --files{"lib/imgui/*.cpp"}
        --includedirs{"lib/imgui"}
        --files{"lib/imgui/backends/*.cpp"}
        symbols "On"        
        includedirs{"lib/glfw/include","lib/glew/include"}
        defines {"GLEW_STATIC"}
        links{"GLEW","glfw3","GL","X11"}
        libdirs{"lib/glfw/lib","lib/glew/lib"}
    filter "configurations:Release"
        defines {"_DOCKER"}
        optimize "On"

    project "AtlasNet"
        kind "SharedLib"
        language "C++"
        files { "src/**.cpp" }
        pchheader "src/pch.hpp"
        pchsource "src/pch.cpp"
    project "God"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files {"srcRun/GodRun.cpp" }
        defines "_GOD"
   project "GodView"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files {"srcRun/GodViewRun.cpp" }
        defines "_GODVIEW"
    project "Partition"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files {"srcRun/PartitionRun.cpp" }
        defines "_PARTITION"

    project "SampleGame"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "examples/SampleGame/**.cpp" }
        defines {"_GAMECLIENT","_GAMESERVER"}

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
    local dirs = {"bin","obj","build","docker"};
    local files = {}
    customClean(dirs,files)
end
function CleanDeps()
     local dirs = {"vcpkg","vcpkg_installed"};
    local files = {}
    customClean(dirs,files)
end
function CleanDocs()
     local dirs = {"docs"};
    local files = {}
    customClean(dirs,files)
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
    execute = function ()
        --local vcpkgDir = path.join(os.getenv("HOME") or ".", "vcpkg")
        local manifestDir = os.getcwd()
         os.execute("git clone https://github.com/microsoft/vcpkg.git ")
         os.execute("./vcpkg/bootstrap-vcpkg.sh")
         os.execute("./vcpkg/vcpkg install")
    end
}
newaction
{
    trigger = "GenDocs",
    description = "generate documentation",
    execute = function ()
        os.execute("doxygen Doxyfile")
    end

}
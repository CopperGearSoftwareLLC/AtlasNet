-- premake5.lua

workspace "GuacNet"
    architecture "x86_64"
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
    
    links { "boost_container", "curl", "GameNetworkingSockets", "GLEW", "glfw3", "glm", "imgui","implot","implot3d","GL" }

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
        optimize "On"

    project "AtlasNet"
        kind "StaticLib"
        language "C++"
        files { "src/**.cpp" }
        pchheader "src/pch.hpp"
        pchsource "src/pch.cpp"
    project "God"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/GodRun.cpp" }
        defines "_GOD"
    project "GodView"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/GodViewRun.cpp" }
        defines "_GODVIEW"
    project "Partition"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/PartitionRun.cpp" }
        defines "_PARTITION"

    project "UnitTests"
        dependson "AtlasNet"
        links "AtlasNet"
        kind "ConsoleApp"
        language "C++"
        files { "Tests/SampleGame/**.cpp" }
        defines { "_GAMECLIENT", "_GAMESERVER" }

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
function BuildDockerImage(buildTarget, macros)
    -- Simple Dockerfile template with macros
    local devcontainerjson = [[
    {
  "name": "]]..buildTarget..[[",
  "remoteUser": "root",
   "customizations": {
    "vscode": {
      "extensions": ["streetsidesoftware.code-spell-checker","ms-vscode.cpptools-extension-pack","augustocdias.tasks-shell-input"],
      "settings": [{ "terminal.integrated.defaultProfile.linux": "bash"}]
    }
  },
  "postCreateCommand": "touch test.txt"
}
    ]]
    local launchtask = [[
   {
    "version": "0.2.0",
    "configurations": [
       {
      "name": "Attach",
      "type": "cppdbg",
        "request": "attach",

      "program": "${workspaceFolder}/bin/${BUILD_CONFIG}/${BUILD_TARGET}/${BUILD_TARGET}",
      "processId": "${input:findPid}",
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb",
    "miDebuggerPath": "/usr/bin/gdb",
      "stopAtEntry": false,
      "useExtendedRemote": true
      
    },
    ],
"inputs": [
  {
    "id": "findPid",
    "type": "command",
    "command": "shellCommand.execute",
    "args": {
      "command": "pidof ${BUILD_TARGET}",   // can be any script or inline shell,
      "useFirstResult" : true
    }
  }
]
}
    ]]
    for key, value in pairs(macros) do
        devcontainerjson = devcontainerjson:gsub("%${" .. key .. "}", value)
        launchtask = launchtask:gsub("%${" .. key .. "}", value)
    end
    local devcontainerFile = io.open("docker/"..buildTarget.."/devcontainer.json", "w")
    devcontainerFile:write(devcontainerjson)
    devcontainerFile:close()
    local LaunchJsonFile = io.open("docker/"..buildTarget.."/launch.json","w")
    LaunchJsonFile:write(launchtask)
    LaunchJsonFile:close()
    local BaseTemplateBuild = [[
FROM ubuntu:25.04

WORKDIR /app

RUN apt-get update && apt-get install git gdbserver gdb curl zip less unzip ccache coreutils tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev -y


RUN git clone https://github.com/microsoft/vcpkg.git
RUN ./vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_DEFAULT_TRIPLET=x64-linux
ENV VCPKG_FEATURE_FLAGS=manifests
COPY vcpkg.json /app
#ARG HOST_HOME
RUN ls -R / | less

#RUN --mount=type=bind,source=${HOST_HOME}/.cache/vcpkg,target=/root/.cache/vcpkg 
RUN --mount=type=cache,target=/root/.cache/vcpkg \
./vcpkg/vcpkg install
RUN ls -R / | less
ENV LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux/lib

RUN git clone https://github.com/premake/premake-core.git
WORKDIR /app/premake-core
RUN make -f Bootstrap.mak linux
WORKDIR /app
COPY docker/]]..buildTarget..[[/devcontainer.json .devcontainer/devcontainer.json
COPY docker/]]..buildTarget..[[/launch.json .vscode/launch.json
# COPY premake5 /app
COPY premake5.lua /app
COPY vcpkg.json /app

COPY Tests /app
COPY src /app/src
COPY srcRun /app/srcRun
RUN ./premake-core/bin/release/premake5 gmake
RUN --mount=type=cache,target=/app/obj \
    make -C ./build ${BUILD_TARGET} config=${BUILD_CONFIG_LC} -j $(nproc)

#CMD ["sh", "-c", "find / -path '*/.cache/vcpkg*' 2>/dev/null | less"]
#CMD ["top"]
#CMD ["ls","/.."]
ENTRYPOINT ["bin/${BUILD_CONFIG}/${BUILD_TARGET}/${BUILD_TARGET}"] 


#CMD ["${RUN_CMD}"]
]]
local BaseTemplate = [[
FROM ubuntu:25.04

WORKDIR /app

RUN apt-get update && apt-get install gdbserver git curl zip unzip tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev -y

ENV LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux/lib
COPY vcpkg_installed/ /app/vcpkg_installed/
RUN apt-get purge -y libprotobuf-dev protobuf-compiler

${COPY_AND_RUN_CMDS}
]]

    
    -- Replace macros in template
    for key, value in pairs(macros) do
        BaseTemplateBuild = BaseTemplateBuild:gsub("%${" .. key .. "}", value)
    end

    local dockerFilePath = "docker/dockerfile." .. string.lower(buildTarget)
    -- Write to file
    local f = io.open(dockerFilePath, "w")
    if f then
        f:write(BaseTemplateBuild)
        f:close()
        print("Generated Dockerfile at " .. dockerFilePath)
    else
        print("Failed to open " .. dockerFilePath)
    end
    os.execute("DOCKER_BUILDKIT=1 docker build --build-arg HOST_HOME=$HOME -f "..dockerFilePath.." -t ".. string.lower(buildTarget)..":latest .")
end

function BuildDockerImageFromTarget(target,build_config,app_bundle)
     
    if target == "God" or target == "Demigod"or target == "GameCoordinator" then
        BuildF(string.lower(build_config),target)
        BuildDockerImage(target, {
            BUILD_TARGET = target,
            BUILD_CONFIG = (build_config),
            BUILD_CONFIG_LC = string.lower(build_config),
            RUN_CMD = target,
        COPY_AND_RUN_CMDS = [[COPY bin/]] .. build_config .."/".. target.."/"..target.." /app\n" ..
        "ENTRYPOINT [\"/app/"..target.."\"]"
    })
    elseif target == "Partition" then
      
        BuildF(string.lower(build_config),target .." ".. app_bundle)
        BuildDockerImage(target, {
            RUN_CMD = "/usr/bin/sh -c ls -a",
            BUILD_TARGET = target,
            BUILD_CONFIG = (build_config),
            BUILD_CONFIG_LC = string.lower(build_config),
        COPY_AND_RUN_CMDS = "COPY bin/" .. build_config .."/".. target.."/"..target.." /app\n" ..
        "COPY bin/" .. build_config .."/".. app_bundle.."/"..app_bundle.." /app\n" ..
        "ENTRYPOINT [\"/app/"..target.."\"]"
    })
    end
end
newaction {
    trigger = "build-docker-image",
    description = "generate documentation",
    execute = function()
        local target = _OPTIONS["target"] or "INVALID"
        local build_config = _OPTIONS["build-config"] or "Release"
        local app_bundle = _OPTIONS["app-bundle"] or "INVALID"
        BuildDockerImageFromTarget(target,build_config,app_bundle)
    end
}
function RunDockerImage(_target,_build_config,_app_bundle)
     local target = _target or _OPTIONS["target"] or "INVALID"
        local build_config = _build_config or _OPTIONS["build-config"] or "Release"
        local app_bundle = _app_bundle or _OPTIONS["app-bundle"] or "INVALID"
        BuildDockerImageFromTarget(target,build_config,app_bundle)
        os.execute("docker rm -f " .. target .. " >/dev/null 2>&1; ")
        local ok, _, code = os.execute("docker run --init -v /var/run/docker.sock:/var/run/docker.sock "..
        "--cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name "..target.." -d -p 1234:1234 "..string.lower(target).." UnitTest=" ..(_OPTIONS["testid"] or "-1") )
        if not ok or code ~= 0 then
            error("(exit code: " .. tostring(code) .. ")", 2)
        end
end
newaction {
    trigger = "run-docker-image",
    description = "generate documentation",
    execute = function()
       RunDockerImage()
    end
}
function BuildF(build_config,target)
    local pok, _, pcode = os.execute("./premake5 gmake")
    if not pok or pcode ~= 0 then
        error("(exit code: " .. tostring(pcode) .. ")", 2)
    end
    local handle = io.popen("nproc --all")
    local result = handle:read("*a")
    handle:close()
    local cores = tonumber(result:match("%d+"))
    print("Compiling in " .. cores .. " cores")
    local mok, _, mcode = os.execute("make -C build config=" .. (build_config) .." ".. (target) .. " -j "..cores)
    if not mok or mcode ~= 0 then
        error("(exit code: " .. tostring(mcode) .. ")", 2)
    end
end
newaction {
    trigger = "Build",
    description = "build",
    execute = function()
         BuildF(string.lower(_OPTIONS["build-config"] or "Release"),(_OPTIONS["target"] or ""))
    end
}
newaction {
    trigger = "AtlasNetStart",
    description = "build",
    execute = function()
         BuildDockerImageFromTarget("Partition","DebugDocker",_OPTIONS["app-bunble"] or "UnitTests")
         RunDockerImage("God","DebugDocker")
    end
}
newoption {
    trigger     = "target",
    value       = "NAME",
    description = "Specify which target to generate for"
}
newoption {
    trigger     = "build-config",
    value       = "Release",
    description = "Specify which build to generate for"
}
newoption {
    trigger = "app-bundle",
    value = "Invalid",
    description = "Specify the app to bundle with partition"
}
newoption {
    trigger = "testid",
    value = "-1",
    description = "Specify the id to run from unit tests"
}
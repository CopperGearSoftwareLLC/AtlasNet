-- premake5.lua
local _PORT_GOD = 25564
local _PORT_PARTITION = 25565
local _PORT_GAMESERVER = 25566
local DevPackages = "git tini gdbserver build-essential binutils automake libtool m4 autoconf gdb curl zip less unzip ccache coreutils tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev"
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
    
    links { "boost_stacktrace_addr2line","boost_container", "curl", "GameNetworkingSockets", "GLEW", "glfw3", "glm", "imgui","implot","implot3d","GL","curl","ssl","crypto","z","dl","redis++","hiredis" }
    defines {"BOOST_STACKTRACE_LINK","BOOST_STACKTRACE_USE_ADDR2LINE"}


    defines {"_PORT_GOD=".._PORT_GOD,
    "_PORT_PARTITION=".._PORT_PARTITION,
    "_PORT_GAMESERVER=".._PORT_GAMESERVER}

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
        links {"AtlasNet",}
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/PartitionRun.cpp" }
        defines "_PARTITION"
    project "Database"
        dependson "AtlasNet"
        links {"AtlasNet"}
        kind "ConsoleApp"
        language "C++"
        files { "srcRun/DatabaseRun.cpp" }
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
FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install ]]..DevPackages..[[ -y
]]

    -- Inject Redis installation if target is Database
        BaseTemplateBuild = BaseTemplateBuild .. [[
# Install Redis
RUN apt-get update && apt-get install -y redis-server && rm -rf /var/lib/apt/lists/*
]]


BaseTemplateBuild = BaseTemplateBuild .. [[

RUN git clone https://github.com/microsoft/vcpkg.git
RUN ./vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_DEFAULT_TRIPLET=x64-linux
ENV VCPKG_FEATURE_FLAGS=manifests
COPY vcpkg.json /app
#ARG HOST_HOME

#RUN --mount=type=bind,source=${HOST_HOME}/.cache/vcpkg,target=/root/.cache/vcpkg 
RUN --mount=type=cache,target=/root/.cache/vcpkg \
./vcpkg/vcpkg install
RUN apt-get purge -y 'libprotobuf*' 'protobuf-compiler' 'libprotoc*' || true && \
    apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*
ENV LD_LIBRARY_PATH=/app/vcpkg_installed/x64-linux/lib

RUN git clone https://github.com/premake/premake-core.git
WORKDIR /app/premake-core
RUN make -f Bootstrap.mak linux
WORKDIR /app
COPY docker/]]..buildTarget..[[/devcontainer.json .devcontainer/devcontainer.json
COPY docker/]]..buildTarget..[[/launch.json .vscode/launch.json
# COPY premake5 /app
COPY premake5.lua /app

COPY Tests /app/Tests
COPY src  /app/src
COPY srcRun /app/srcRun
RUN  ./premake-core/bin/release/premake5 gmake
RUN \
 --mount=type=cache,target=/app/obj \
    make -C ./build ${BUILD_TARGET} config=${BUILD_CONFIG_LC} -j $(nproc)
 #COPY bin /app/bin


${ENTRYPOINT}



#CMD ["${RUN_CMD}"]
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
    os.execute("DOCKER_BUILDKIT=1 docker build --pull --build-arg HOST_HOME=$HOME -f "..dockerFilePath.." -t ".. string.lower(buildTarget)..":latest .")
end

function BuildDockerImageFromTarget(target,build_config,app_bundle)
     
    
    if target == "God" then
        BuildF(string.lower(build_config),target)
        BuildDockerImage(target, {
            BUILD_TARGET = target,
            BUILD_CONFIG = (build_config),
            BUILD_CONFIG_LC = string.lower(build_config),
            RUN_CMD = target,
            ENTRYPOINT="ENTRYPOINT [\"bin/".. build_config.."/"..target.."/"..target.."\"]",
    })
    elseif target == "Database" then
        BuildF(string.lower(build_config),target)
        BuildDockerImage(target, {
            BUILD_TARGET = target,
            BUILD_CONFIG = (build_config),
            BUILD_CONFIG_LC = string.lower(build_config),
            RUN_CMD = target,
            ENTRYPOINT="ENTRYPOINT [\"bin/".. build_config.."/"..target.."/"..target.."\"]",
    })
    elseif target == "Partition" then
      
        BuildF(string.lower(build_config),target .. " "..app_bundle)
        BuildDockerImage(target, {
            RUN_CMD = "/usr/bin/sh -c ls -a",
            BUILD_TARGET = target .. " "..app_bundle,
            BUILD_CONFIG = (build_config),
            BUILD_CONFIG_LC = string.lower(build_config),
            --ENTRYPOINT = "RUN... \nENTRYPOINT [\"/usr/bin/tini\", \"--\", \"sh\", \"-c\", \"bin/"..build_config.."/"..target.."/"..target.. " \\\"$@\\\" & bin/"..build_config.."/"..app_bundle.."/"..app_bundle.. " \\\"$@\\\"; wait\", \"--\"]",
            ENTRYPOINT = table.concat({
    'RUN printf \'%%s\\n\' "#!/bin/bash" \\',
    '    "set -e" \\',
    '    "trap \\"kill 0\\" SIGINT SIGTERM" \\',
    '    "bin/' .. build_config .. '/' .. target .. '/' .. target .. ' \\"$@\\" &" \\',
    '    "bin/' .. build_config .. '/' .. app_bundle .. '/' .. app_bundle .. ' \\"$@\\" &" \\',
    '    "wait -n" \\',
    '    "kill 0" > /entrypoint.sh && chmod +x /entrypoint.sh',
    'ENTRYPOINT ["/usr/bin/tini", "--", "/entrypoint.sh"]'
}, "\n")
            --ENTRYPOINT = "ENTRYPOINT [\"/usr/bin/tini\",\"--\",\"sh\",\"-c\",\"bin/".. build_config .."/".. target .."/".. target .." & bin/".. build_config .."/".. app_bundle .."/".. app_bundle .."; wait\"]"

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
        local ok, _, code = os.execute("docker run --init --network AtlasNet --stop-timeout=999999 -v /var/run/docker.sock:/var/run/docker.sock "..
        "--cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name "..target.." -d -p :".._PORT_GOD.." "..string.lower(target))--.." UnitTest=" ..(_OPTIONS["testid"] or "-1") )
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

function MakeDockerImages()

         BuildDockerImageFromTarget("Partition","DebugDocker",_OPTIONS["app-bundle"] or "UnitTests")
         BuildDockerImageFromTarget("Database","DebugDocker")
         BuildDockerImageFromTarget("God","DebugDocker")
end
newaction {
    trigger = "Build",
    description = "build",
    execute = function()
         MakeDockerImages()
    end
}
newaction {
    trigger = "AtlasNetStart",
    description = "build",
    execute = function()
      os.execute("docker volume create redis_data")
          MakeDockerImages()
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
    value = "UnitTests",
    description = "Specify the app to bundle with partition"
}
newoption {
    trigger = "testid",
    value = "-1",
    description = "Specify the id to run from unit tests"
}
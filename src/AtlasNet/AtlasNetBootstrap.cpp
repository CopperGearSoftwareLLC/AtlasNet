#include "AtlasNetBootstrap.hpp"
#include "misc/String_utils.hpp"
#include "AtlasNet.hpp"
#include "Docker/DockerIO.hpp"
struct CommandResult
{
    int exitCode;
    std::string output;
};

static CommandResult RunCommand(const std::string &cmd)
{
    std::array<char, 256> buffer;
    std::string result;

    // Redirect stderr to stdout so we capture everything
    std::string fullCmd = cmd + " 2>&1";

    FILE *pipe = popen(fullCmd.c_str(), "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();

    int rc = pclose(pipe);
    int exitCode = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;

    return {exitCode, result};
}
static std::string GetHiddenInput(const std::string &prompt)
{
    std::cout << prompt;
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // get current terminal attributes
    newt = oldt;
    newt.c_lflag &= ~ECHO;                   // disable echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); // apply changes
    std::string input;
    if (std::cin.peek() == '\n')
        std::cin.ignore();
    std::getline(std::cin, input);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restore echo
    std::cout << std::endl;
    return input;
}
AtlasNetBootstrap::AtlasNetBootstrap()
{
}
void AtlasNetBootstrap::Run()
{

    // 1. setup swarm + network + registry
    // EnsureDockerLogin();
    // ClearOldCache();
    RunningLocally = AtlasNet::Get().GetSettings().workers.empty();
    SetupSwarm();
    SetupNetwork();
    GenerateTSLCertificate();

    if (!RunningLocally)
    {
        SetupRegistry();
        GetWorkersSSHCredentials();
        SendTLSCertificateToWorkers();
        JoinWorkerToSwarm();
        MakeDockerBuilder();
    }
    CreateDevContainerJson();
    CreateGodImage();
    CreatePartitionImage();
    CreateDatabaseImage();
    CreateGameCoordinatorImage();
    CreateDemigodImage();
    RunDatabase();
    SetupPartitionService();
    RunGod();
    RunGameCoordinator();
    SetupDemigodService();
    // RunDemigod();
}

std::string AtlasNetBootstrap::MacroParse(std::string input, std::unordered_map<std::string, std::string> macros)
{
    std::regex macroPattern(R"(\$\{([^}]+)\})");
    std::string result;
    std::sregex_iterator it(input.begin(), input.end(), macroPattern);
    std::sregex_iterator end;

    size_t lastPos = 0;
    result.reserve(input.size());

    while (it != end)
    {

        for (; it != end; ++it)
        {
            const std::smatch &match = *it;
            result.append(input, lastPos, match.position() - lastPos);

            std::string key = match[1].str();
            auto found = macros.find(key);
            if (found != macros.end())
            {
                result += found->second;
            }

            else
                result += match.str(); // keep original ${VAR} if not found

            lastPos = match.position() + match.length();
        }
    }

    result.append(input, lastPos, std::string::npos);
    bool fullyParsed = true;
    if (auto it = std::sregex_iterator(result.begin(), result.end(), macroPattern); it != end)
    {
        for (; it != end; ++it)
            if (macros.find(it->str()) == macros.end())
            {
                fullyParsed = false;
            }
        if (fullyParsed)
            result = MacroParse(result, macros);
    };
    return result;
}

void AtlasNetBootstrap::CreateLaunchJson(const std::string &LaunchJsonDir, const std::vector<std::string> &executables)
{
    Ordered_Json j;

    j["version"] = "0.2.0";

    // Configuration section

    for (const auto &executable : executables)
    {
        Ordered_Json config = {
            {"name", "Attach"},
            {"type", "cppdbg"},
            {"request", "attach"},
            {"program", std::format("${{workspaceFolder}}/bin/{}/{}/{}", "DebugDocker", executable, executable)},
            {"processId", std::format("${{input:findPid{}}}", executable)},
            {"cwd", "${workspaceFolder}"},
            {"MIMode", "gdb"},
            {"miDebuggerPath", "/usr/bin/gdb"},
            {"stopAtEntry", false},
            {"useExtendedRemote", true}};

        j["configurations"].push_back(config);
        // Inputs section
        Ordered_Json inputArgs = {
            {"command", std::format("pidof {}", executable)},
            {"useFirstResult", true}};

        Ordered_Json input = {
            {"id", std::format("findPid{}", executable)},
            {"type", "command"},
            {"command", "shellCommand.execute"},
            {"args", inputArgs}};
        j["inputs"].push_back(input);
    }
    const std::string LaunchJsonPath = DockerFilesFolder + "/" + LaunchJsonDir;

    std::filesystem::path p(LaunchJsonPath);
    std::filesystem::create_directories(p.parent_path());
    std::ofstream file(LaunchJsonPath);
    if (!file.is_open())
    {
        throw "FUCK";
    }
    file << j.dump(4);
    file.close();
    logger.DebugFormatted("Outputed launch json to {}", LaunchJsonPath);
}

void AtlasNetBootstrap::CreateDevContainerJson()
{
    const std::string DevContainerPath = DockerFilesFolder + "/devcontainer.json";
    std::filesystem::path p(DevContainerPath);
    std::filesystem::create_directories(p.parent_path());
    std::ofstream file(DevContainerPath);
    if (!file.is_open())
    {
        throw "FUCK";
    }
    file << DevContainerJson.dump(4);
    file.close();
    logger.Debug("Outputed devcontainer.json");
}

void AtlasNetBootstrap::OutputDockerFile(const std::string &Name, const std::string &Content)
{
    const std::string DockerfilePath = DockerFilesFolder + "/" + Name;
    std::filesystem::path p(DockerfilePath);
    std::ofstream file(DockerfilePath);
    if (file.is_open())
    {
        file << Content;
        file.close();
    }
}

void AtlasNetBootstrap::CreateGodImage()
{
    CreateLaunchJson("God/launch.json", {"God"});

    std::string DockerFileContents = DockerImageBase + BuildBinariesInstructions + EntryPointString;
    std::string packages;
    for (const auto &p : RequiredPackages)
    {
        packages += p + " ";
    }
    DockerFileContents = MacroParse(DockerFileContents, {{"OS_VERSION", OS_Version},
                                                         {"ENTRYPOINT_CMD", "\"bin/" + BuildConfig + "/God/God\""},
                                                         {"BUILD_CONFIG", ToLower(BuildConfig)},
                                                         {"WORKDIR", WorkDir},
                                                         {"DEV_PACKAGES", packages},
                                                         {"PROJ_TO_BUILD", "God"},
                                                         {"EXECUTABLE", "God"}});
    OutputDockerFile("God/Dockerfile", DockerFileContents);
    BuildDockerImage("God/Dockerfile", GodImageName, AtlasNet::Get().GetSettings().RuntimeArches);
}

void AtlasNetBootstrap::CreatePartitionImage()
{
    CreateLaunchJson("Partition/launch.json", {"Partition", "UnitTests"});

    std::string DockerFileContents = DockerImageBase + BuildBinariesInstructions;
    DockerFileContents += MacroParse("RUN mv ./bin/${BUILD_CONFIG}/Partition/Partition ./Partition \n", {{"BUILD_CONFIG", BuildConfig}});
    std::string packages;
    for (const auto &p : RequiredPackages)
    {
        packages += p + " ";
    }

    #ifdef _NDEBUG
    for (const auto srcDir : AtlasNet::Get().GetSettings().SourceDirectories)
    {
        DockerFileContents += "COPY " + srcDir + " " + WorkDir +"/"+srcDir+ "\n";
    }

    std::string BuildServerExe;
    for (const auto buildCmd : AtlasNet::Get().GetSettings().GameServerBuildCmds)
    {
        BuildServerExe += "RUN " + buildCmd + "\n";
    }
    ProjToBuild+=BuildServerExe;
    std::string ProjToBuild = "Partition";

   #else
    std::string ProjToBuild = "Partition UnitTestsServer";
   #endif

    const std::string EntryPointCmd = MacroParse(R"(
        RUN cat <<'EOF' > ./entrypoint.sh
#!/bin/bash
set -euo pipefail
chmod +x ./Partition
chmod +x ./${exe2}
./Partition &
pid1=$!
${exe2} &
pid2=$!

term_handler() {
    echo "Received SIGTERM or SIGINT, forwarding to children..."
    kill -TERM "$pid1" 2>/dev/null || true
    kill -TERM "$pid2" 2>/dev/null || true
}

trap term_handler SIGTERM SIGINT

wait -n
exit_code=$?

echo "One process exited (code $exit_code), stopping the rest..."
kill -TERM "$pid1" "$pid2" 2>/dev/null || true

wait
exit $exit_code
EOF
RUN chmod +x ./entrypoint.sh
ENTRYPOINT ["/usr/bin/tini", "--", "./entrypoint.sh"])",
                                                 {{"exe1", "bin/" + BuildConfig + "/Partition/Partition"}, {"exe2", *AtlasNet::Get().GetSettings().GameServerRunCmds.begin()}});
    DockerFileContents += EntryPointCmd;
    DockerFileContents = MacroParse(DockerFileContents, {{"OS_VERSION", OS_Version},
                                                         {"BUILD_CONFIG", ToLower(BuildConfig)},
                                                         {"WORKDIR", WorkDir},
                                                         {"PROJ_TO_BUILD", ProjToBuild},
                                                         {"DEV_PACKAGES", packages},
                                                         {"EXECUTABLE", "Partition"}});

    OutputDockerFile("Partition/Dockerfile", DockerFileContents);
    BuildDockerImage("Partition/Dockerfile", PartitionImageName, AtlasNet::Get().GetSettings().RuntimeArches);
}

void AtlasNetBootstrap::CreateDatabaseImage()
{
    CreateLaunchJson("Database/launch.json", {"Database"});

    std::string DockerFileContents = DockerImageBase + BuildBinariesInstructions + EntryPointString;
    std::string packages;
    for (const auto &p : RequiredPackages)
    {
        packages += p + " ";
    }
    DockerFileContents = MacroParse(DockerFileContents, {{"OS_VERSION", OS_Version},
                                                         {"ENTRYPOINT_CMD", "\"bin/" + BuildConfig + "/Database/Database\""},
                                                         {"BUILD_CONFIG", ToLower(BuildConfig)},
                                                         {"WORKDIR", WorkDir},
                                                         {"PROJ_TO_BUILD", "Database"},
                                                         {"DEV_PACKAGES", packages},
                                                         {"EXECUTABLE", "Database"}});
    OutputDockerFile("Database/Dockerfile", DockerFileContents);
    BuildDockerImage("Database/Dockerfile", DatabaseImageName, AtlasNet::Get().GetSettings().RuntimeArches);
}

void AtlasNetBootstrap::CreateGameCoordinatorImage()
{
    CreateLaunchJson("GameCoordinator/launch.json", {"GameCoordinator"});

    std::string dockerfile =
        DockerImageBase + BuildBinariesInstructions + EntryPointString;

    std::string packages;
    for (const auto &pkg : RequiredPackages)
        packages += pkg + " ";

    dockerfile = MacroParse(dockerfile, {{"OS_VERSION", OS_Version},
                                         {"ENTRYPOINT_CMD", "\"bin/" + BuildConfig + "/GameCoordinator/GameCoordinator\""},
                                         {"BUILD_CONFIG", ToLower(BuildConfig)},
                                         {"WORKDIR", WorkDir},
                                         {"DEV_PACKAGES", packages},
                                         {"PROJ_TO_BUILD", "GameCoordinator"},
                                         {"EXECUTABLE", "GameCoordinator"}});

    OutputDockerFile("GameCoordinator/Dockerfile", dockerfile);
    BuildDockerImage("GameCoordinator/Dockerfile",
                     GameCoordinatorImageName,
                     AtlasNet::Get().GetSettings().RuntimeArches);
}

void AtlasNetBootstrap::CreateDemigodImage()
{
    CreateLaunchJson("Demigod/launch.json", {"Demigod"});

    std::string dockerfile =
        DockerImageBase + BuildBinariesInstructions + EntryPointString;

    std::string packages;
    for (const auto &pkg : RequiredPackages)
        packages += pkg + " ";

    dockerfile = MacroParse(dockerfile, {{"OS_VERSION", OS_Version},
                                         {"ENTRYPOINT_CMD", "\"bin/" + BuildConfig + "/Demigod/Demigod\""},
                                         {"BUILD_CONFIG", ToLower(BuildConfig)},
                                         {"WORKDIR", WorkDir},
                                         {"DEV_PACKAGES", packages},
                                         {"PROJ_TO_BUILD", "Demigod"},
                                         {"EXECUTABLE", "Demigod"}});

    OutputDockerFile("Demigod/Dockerfile", dockerfile);
    BuildDockerImage("Demigod/Dockerfile",
                     DemigodImageName,
                     AtlasNet::Get().GetSettings().RuntimeArches);
}

bool AtlasNetBootstrap::BuildDockerImage(const std::string &DockerFile, const std::string &ImageName, const std::unordered_set<std::string> &arches)
{
    const std::string registry = ManagerPcAdvertiseAddr + ":5000";
    std::vector<std::string> fullTags;
    std::vector<std::string> imageDigests;
    for (const auto &arch : arches)
    {
        std::string archn = arch;
        archn.erase(std::remove_if(archn.begin(), archn.end(), ::isspace), archn.end());

        std::string tag;
        if (!RunningLocally)
        {

            tag = std::format("{}/{}:{}", registry, ImageName, archn);
        }
        else
        {
            tag = ImageName;
        }
        fullTags.push_back(tag);

        // Build the image
        const auto &cacheDir = AtlasNet::Get().GetSettings().BuildCacheDir;
        std::string BuildCacheArgument = cacheDir.empty() ? "" : "--cache-from type=local,src=" + cacheDir + " --cache-to type=local,dest=" + cacheDir + ",mode=max";
        std::string buildCmd;
        if (RunningLocally)
        {
            buildCmd = std::format(
                "docker build "
                "-f {}/{} "
                "-t {} "
                "{} .",
                DockerFilesFolder, DockerFile, tag, "");
        }
        else
        {
            buildCmd = std::format(
                "docker buildx build --builder {} "
                "--platform linux/{} "
                "--provenance=false "
                "-f {}/{} -t {} "
                "{} --push .",
                BuilderName, archn, DockerFilesFolder, DockerFile, tag, BuildCacheArgument);
        }

        logger.DebugFormatted("🏗️ Building image '{}' for arch '{}' using '{}'", ImageName, archn, DockerFile);
        if (std::system(buildCmd.c_str()) != 0)
        {
            logger.ErrorFormatted("❌ Failed to build image '{}' for arch '{}'", ImageName, archn);
            throw std::runtime_error("Docker build failed");
        }

        logger.DebugFormatted("✅ Successfully built & pushed '{}' (arch '{}')", ImageName, archn);
    }
    if (!RunningLocally)
    {
        // After building all architecture images, create and push a manifest list
        std::string manifestTag = std::format("{}/{}:latest", registry, ImageName);

        logger.DebugFormatted("🧩 Creating manifest list '{}'", manifestTag);

        // Build the manifest creation command
        std::string manifestCmd = std::format("docker manifest create --amend {}", manifestTag);
        for (const auto &fullTag : fullTags)
            manifestCmd += std::format(" {}", fullTag);

        // Create manifest
        if (std::system(manifestCmd.c_str()) != 0)
        {
            logger.ErrorFormatted("❌ Failed to create manifest list '{}'", manifestTag);
            throw std::runtime_error("Manifest creation failed");
        }

        // Push the manifest list
        if (std::system(std::format("docker manifest push {}", manifestTag).c_str()) != 0)
        {
            logger.ErrorFormatted("❌ Failed to push manifest list '{}'", manifestTag);
            throw std::runtime_error("Manifest push failed");
        }

        logger.DebugFormatted("✅ Successfully pushed multi-arch manifest '{}'", manifestTag);
    }

    return true;
}

void AtlasNetBootstrap::QueueDockerImage(const std::string &DockerFile, const std::string &ImageName, const std::unordered_set<std::string> &arches)
{
    const std::string registry = ManagerPcAdvertiseAddr + ":5000";
    const std::string bakeFile = "docker/bake.json"; // stored in working directory

    Json bakeJson;

    // Load existing bake.json if it exists
    if (std::filesystem::exists(bakeFile))
    {
        std::ifstream in(bakeFile);
        in >> bakeJson;
    }
    else
    {
        bakeJson["group"]["default"]["targets"] = Json::array();
        bakeJson["target"] = Json::object();
    }

    // Add one target per arch
    for (const auto &arch : arches)
    {
        std::string cleanArch = arch;
        cleanArch.erase(std::remove_if(cleanArch.begin(), cleanArch.end(), ::isspace), cleanArch.end());
        std::string fullTag = std::format("{}/{}:{}", registry, ImageName, cleanArch);

        Json target;
        target["context"] = ".";
        target["dockerfile"] = std::format("{}/{}", DockerFilesFolder, DockerFile);
        target["platforms"] = Json::array({std::format("linux/{}", cleanArch)});
        target["tags"] = Json::array({fullTag});
        target["provenance"] = false;

        const auto &cacheDir = AtlasNet::Get().GetSettings().BuildCacheDir + "/" + ImageName + "/" + arch;
        if (!cacheDir.empty())
        {
            target["cache-from"] = {std::format("type=local,src={}", cacheDir)};
            target["cache-to"] = {std::format("type=local,dest={},mode=min", cacheDir)};
        }

        // Unique name for this target
        std::string targetName = std::format("{}_{}", ImageName, cleanArch);
        bakeJson["target"][targetName] = target;
        if (!bakeJson["group"]["default"]["targets"].contains(targetName))
        {
            bakeJson["group"]["default"]["targets"].push_back(targetName);
        }

        logger.DebugFormatted("📦 Queued bake target '{}'", targetName);
    }

    // Save updated bake.json
    std::ofstream out(bakeFile);
    out << std::setw(4) << bakeJson;

    logger.DebugFormatted("📝 Added '{}' targets to bake.json", ImageName);
}
void AtlasNetBootstrap::RunGod()
{
    // Remove old container OR service safely
    system("docker rm -f God > /dev/null 2>&1");
    system("docker service rm God > /dev/null 2>&1");

    logger.Debug("Running God");

    std::string imageName = RunningLocally
                                ? GodImageName
                                : (ManagerPcAdvertiseAddr + std::string(":5000/") + GodImageName);

    // Create the Swarm service
    std::string command =
        "docker service create "
        "--name God "
        "--replicas 1 "
        "--restart-condition on-failure "
        "--restart-max-attempts 0 "
        "--restart-delay 2s "
        "--network " +
        NetworkName + " "
                      "--mount type=bind,src=/var/run/docker.sock,dst=/var/run/docker.sock "
                      "--cap-add SYS_PTRACE "
                      // publish TCP
                      "--publish published=" +
        std::to_string(_PORT_GOD) +
        ",target=" + std::to_string(_PORT_GOD) + ",protocol=tcp "
                                                 // publish UDP
                                                 "--publish published=" +
        std::to_string(_PORT_GOD) +
        ",target=" + std::to_string(_PORT_GOD) + ",protocol=udp " + imageName;

    system(command.c_str());
}

void AtlasNetBootstrap::RunDatabase()
{
    logger.Debug("Running Database");
    system("docker rm -f Database");

    // Create the container
    std::string ImageName = RunningLocally ? DatabaseImageName : (ManagerPcAdvertiseAddr + ":5000/") + DatabaseImageName;
    std::string runCommand =
        "docker create --name Database --network " + NetworkName + " -v redis_data:/data -p 6379:6379 " + ImageName + " /bin/bash -c \"redis-server --appendonly yes & ./Database\"";
    system(runCommand.c_str());

    // Start it
    system("docker start Database");
}

void AtlasNetBootstrap::RunGameCoordinator()
{
    system("docker rm -f GameCoordinator");

    std::string image =
        RunningLocally ? GameCoordinatorImageName : ManagerPcAdvertiseAddr + ":5000/" + GameCoordinatorImageName;

    std::string cmd =
        "docker run --init --network " + NetworkName +
        " -v /var/run/docker.sock:/var/run/docker.sock --name GameCoordinator -d " +
        image;

    system(cmd.c_str());
}

void AtlasNetBootstrap::RunDemigod()
{
    system("docker rm -f Demigod");

    std::string image =
        RunningLocally ? DemigodImageName : ManagerPcAdvertiseAddr + ":5000/" + DemigodImageName;

    std::string cmd =
        "docker run --init --network " + NetworkName +
        " -v /var/run/docker.sock:/var/run/docker.sock --name Demigod -d " +
        image;

    system(cmd.c_str());
}

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>

void AtlasNetBootstrap::SetupSwarm()
{
    auto isPortInUse = [](int port) -> bool
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
            return true;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("0.0.0.0");
        addr.sin_port = htons(port);
        bool inUse = bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0;
        close(sock);
        return inUse;
    };

    // Pick a free port starting from 2377
    int swarmPort = 2377;
    while (isPortInUse(swarmPort))
    {
        logger.WarningFormatted("⚠️ Port {} already in use, trying {}", swarmPort, swarmPort + 1);
        swarmPort++;
        if (swarmPort > 2400)
            throw std::runtime_error("No available ports found for swarm init (2377–2400)");
    }

    system("docker swarm leave --force >/dev/null 2>&1");

    // Check if already a manager
    std::string infoResp = DockerIO::Get().request("GET", "/info");
    auto infoJson = Json::parse(infoResp);
    if (infoJson["Swarm"]["LocalNodeState"] == "active" && infoJson["Swarm"]["ControlAvailable"] == true)
    {
        logger.Debug("Already a swarm manager — skipping init.");
        return;
    }

    logger.DebugFormatted("🧩 Initializing Docker Swarm on port {}", swarmPort);

    // Resolve configured network interface
    std::string preferredInterface = AtlasNet::Get().GetSettings().NetworkInterface;
    if (RunningLocally)
        preferredInterface = "lo"; // if running locally then use the loop back interface 127.0.0.1
    std::string interfaceIP;

    // Collect available interfaces
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return;
    }

    struct InterfaceInfo
    {
        std::string name;
        std::string ip;
    };
    std::vector<InterfaceInfo> available;

    for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        char addr[INET_ADDRSTRLEN];
        void *in_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, in_addr, addr, sizeof(addr));

        available.push_back({ifa->ifa_name, addr});
    }
    freeifaddrs(ifaddr);

    // Try preferred interface if declared
    if (!preferredInterface.empty())
    {
        auto it = std::find_if(available.begin(), available.end(),
                               [&](const InterfaceInfo &inf)
                               { return inf.name == preferredInterface; });

        if (it != available.end())
        {
            interfaceIP = it->ip;
            logger.DebugFormatted("✅ Using configured interface '{}' with IP '{}'", preferredInterface, interfaceIP);
        }
        else
        {
            logger.WarningFormatted("⚠️ Configured interface '{}' not found or has no IPv4 address.", preferredInterface);
        }
    }

    // Ask user if needed
    if (interfaceIP.empty())
    {
        if (available.empty())
        {
            logger.Error("❌ No valid network interfaces with IPv4 addresses found.");
            return;
        }

        std::cout << "\nAvailable network interfaces:\n";
        for (size_t i = 0; i < available.size(); ++i)
            std::cout << "[" << i << "] " << available[i].name << " — " << available[i].ip << "\n";

        std::cout << "\nSelect interface number: ";
        int choice = -1;
        std::cin >> choice;

        if (choice < 0 || choice >= static_cast<int>(available.size()))
        {
            logger.Error("❌ Invalid selection.");
            return;
        }

        interfaceIP = available[choice].ip;
        preferredInterface = available[choice].name;

        logger.DebugFormatted("✅ Selected interface '{}' with IP '{}'", preferredInterface, interfaceIP);
    }

    // Construct swarm init payload
    Json swarmInit = Json{
        {"ListenAddr", std::format("0.0.0.0:{}", swarmPort)},
        {"AdvertiseAddr", std::format("{}:{}", interfaceIP, swarmPort)}};

    // Attempt swarm init
    std::string initResult = DockerIO::Get().request("POST", "/swarm/init", &swarmInit);
    Json parsed;
    try
    {
        parsed = Json::parse(initResult);
    }
    catch (...)
    {
        logger.ErrorFormatted("Invalid swarm init response: {}", initResult);
        return;
    }

    if (parsed.contains("message"))
    {
        logger.ErrorFormatted("❌ Swarm init failed: {}", parsed["message"].get<std::string>());
        return;
    }

    logger.DebugFormatted("Swarm init successful:\n{}", parsed.dump(4));

    // Refresh info to capture advertise address
    infoResp = DockerIO::Get().request("GET", "/info");
    infoJson = Json::parse(infoResp);
    ManagerPcAdvertiseAddr = infoJson["Swarm"]["NodeAddr"];
    logger.DebugFormatted("📡 Manager address set to {}", ManagerPcAdvertiseAddr);
}

void AtlasNetBootstrap::SetupNetwork()
{
    // Delete existing network if it exists
    system("docker network rm AtlasNet > /dev/null 2>&1");
    std::string NetworkCommand = std::format("docker network create {} --attachable {}", "--driver overlay", NetworkName);
    auto netResult = RunCommand(NetworkCommand.c_str());
    // 256 = network already exists
    if (netResult.exitCode != 0)
    {
        logger.ErrorFormatted("Failed to create network. Error code {}", netResult.output);
        throw std::runtime_error("Failed to create network");
    }
    else
    {
        logger.DebugFormatted("✅ AtlasNet Overlay network created");
    }
}

void AtlasNetBootstrap::ParallelBuild()
{
    const std::string bakeFile = "docker/bake.json";
    if (!std::filesystem::exists(bakeFile))
    {
        logger.Error("❌ No bake.json file found. Nothing to build.");
        throw "Shit";
    }

    logger.Debug("🔥 Running docker buildx bake for all queued images...");

    std::string cmd = std::format("docker buildx bake --builder {} --push --provenance=false --file {}", BuilderName, bakeFile);
    if (std::system(cmd.c_str()) != 0)
    {
        logger.Error("❌ Bake failed.");
        throw "Shit";
    }

    logger.Debug("✅ All docker images built and pushed successfully.");

    // (Optional) Clean up bake.json if you want a fresh slate
    // std::filesystem::remove(bakeFile);
}

void AtlasNetBootstrap::JoinWorkerToSwarm()
{
    if (AtlasNet::Get().GetSettings().workers.empty())
    {
        logger.Debug("No workers specified. running of this machine");
        return;
    }

    std::string tokenResp = DockerIO::Get().request("GET", "/swarm");
    auto tokenJson = Json::parse(tokenResp);
    std::string joinworkerToken = tokenJson["JoinTokens"]["Worker"];
    logger.DebugFormatted("Docker swarm worker join token {} with managerIP {}", joinworkerToken, ManagerPcAdvertiseAddr);

    std::string sshKeyDir = "keys";
    std::filesystem::create_directories(sshKeyDir);

    for (const auto &onlineworker : onlineWorkers)
    {
        const auto &worker = onlineworker.worker;

        // 1. Ensure remote leaves any existing swarm
        try
        {
            DockerIO remote("http://" + worker.IP + ":2375");
            remote.request("POST", "/swarm/leave?force=true");
        }
        catch (const std::exception &e)
        {
            logger.WarningFormatted("Remote {} may not have been part of a swarm: {}", worker.IP, e.what());
        }

        // 2. Prepare join body for the remote
        Json joinBody = {
            {"RemoteAddrs", Json::array({ManagerPcAdvertiseAddr + ":2377"})},
            {"JoinToken", joinworkerToken},
            {"ListenAddr", "0.0.0.0:2377"},
            {"AdvertiseAddr", worker.IP}};

        // 3. Force the remote to join the manager's swarm
        try
        {
            DockerIO remote("http://" + worker.IP + ":2375");
            std::string resp = remote.request("POST", "/swarm/join", &joinBody);
            logger.DebugFormatted("Remote {} successfully joined swarm:\n{}", worker.IP, resp);
        }
        catch (const std::exception &e)
        {
            logger.ErrorFormatted("❌ Failed to join remote {} to swarm: {}", worker.IP, e.what());
            continue;
        }
        logger.DebugFormatted("✅ joined worker {} at {} to swarm", worker.Name, worker.IP);
    }
}

void AtlasNetBootstrap::SendImagesToWorkers(const std::vector<std::string> &imageNames)
{
    std::string sshKeyDir = "keys";
    std::filesystem::create_directories(sshKeyDir);

    // Step 1. Save all Docker images once
    std::vector<std::pair<std::string, std::string>> savedImages; // {originalName, tarFile}
    for (const auto &image : imageNames)
    {
        std::string safeImageName = image;
        std::replace(safeImageName.begin(), safeImageName.end(), '/', '_');
        std::replace(safeImageName.begin(), safeImageName.end(), ':', '_');
        std::string tarFile = sshKeyDir + "/" + safeImageName + ".tar";
        std::string metaFile = sshKeyDir + "/" + safeImageName + ".meta";
        CommandResult inspectRes = RunCommand(
            "docker image inspect \"" + image + "\" --format='{{.Id}} {{.Created}} {{.Size}}'");
        if (inspectRes.exitCode != 0 || inspectRes.output.empty())
        {
            logger.ErrorFormatted("❌ Failed to inspect image {}:\n{}", image, inspectRes.output);
            continue;
        }
        std::istringstream iss(inspectRes.output);
        std::string imageId, createdAt, imageSize;
        iss >> imageId >> createdAt >> imageSize;
        /*
        bool ShouldRepack = true;
        if (std::filesystem::exists(tarFile) && std::filesystem::exists(metaFile))
        {
            std::ifstream meta(metaFile);
            std::string savedId;
            std::getline(meta, savedId);

            if (savedId == imageId)
            {
                logger.DebugFormatted("✅ {} is up to date. Skipping repack.", image);
                ShouldRepack = false;
            }
        }
        if (!ShouldRepack)
            continue;
*/
        imageSize.erase(imageSize.find_last_not_of(" \n\r\t") + 1);
        logger.DebugFormatted("📦 Saving Docker image {} to {}", image, tarFile);
        std::string saveCmd =
            "docker save \"" + image + "\" | "
                                       "docker run --rm -i "
                                       "python:3.11-slim bash -c 'pip install -q --no-input --disable-pip-version-check --no-warn-script-location tqdm >/dev/null 2>&1 && python -m tqdm --bytes --total " +
            imageSize + "' | "
                        "gzip > \"" +
            tarFile + "\"";
        int saveResult = system(saveCmd.c_str());
        if (saveResult != 0)
        {
            logger.ErrorFormatted("❌ Failed to save Docker image {}", image);
            continue;
        }
        std::ofstream meta(metaFile);
        meta << imageId << std::endl;
        savedImages.emplace_back(image, tarFile);
    }

    // Step 2. Send saved images to all workers
    const auto &workers = AtlasNet::Get().GetSettings().workers;
    std::for_each(std::execution::par_unseq, workers.begin(), workers.end(), [&](const Worker &worker)
                  {

// --- Load saved SSH username ---
        std::string userFile = sshKeyDir + "/user_" + worker.Name + ".txt";
        std::replace(userFile.begin(), userFile.end(), ' ', '_');

        std::string sshUser = "root";
        if (std::filesystem::exists(userFile))
        {
            std::ifstream in(userFile);
            std::getline(in, sshUser);
            in.close();
        }

        std::string keyName = "id_rsa_" + worker.Name;
        std::replace(keyName.begin(), keyName.end(), ' ', '_');
        std::string privateKeyPath = sshKeyDir + "/" + keyName;

        logger.DebugFormatted("🚀 Sending saved Docker images to worker {} ({}) as user {}", worker.Name, worker.IP, sshUser);

        for (const auto &[image, tarFile] : savedImages)
        {
            std::string safeImageName = std::filesystem::path(tarFile).filename().string();
            std::replace(safeImageName.begin(), safeImageName.end(), ' ', '_');

            // --- Copy tarball ---
            std::string scpCmd =
                "scp -i \"" + privateKeyPath + "\" -o StrictHostKeyChecking=no \"" + tarFile + "\" " +
                sshUser + "@" + worker.IP + ":/tmp/";
            logger.DebugFormatted("📤 Transferring {} to {}", tarFile, worker.IP);
            int scpResult = system(scpCmd.c_str());
            if (scpResult != 0)
            {
                logger.ErrorFormatted("❌ Failed to send {} to {}", tarFile, worker.IP);
                continue;
            }

            // --- Load on remote ---
           std::string loadCmd =
    "ssh -i \"" + privateKeyPath + "\" -o StrictHostKeyChecking=no " + sshUser + "@" + worker.IP + " \""
    "gzip -dc /tmp/" + safeImageName + " | "
    "docker run --rm -i "
    "python:3.11-slim bash -c 'pip install -q --no-input --disable-pip-version-check --no-warn-script-location tqdm >/dev/null 2>&1 && python -m tqdm --bytes' | "
    "docker load && "
    "rm -f /tmp/" + safeImageName + "\"";

            logger.DebugFormatted("⚙️ Loading image {} on {}@{}", image, worker.Name, worker.IP);
            int loadResult = system(loadCmd.c_str());
            if (loadResult != 0)
            {
                logger.ErrorFormatted("❌ Failed to load image {} on {}", image, worker.IP);
                continue;
            }

            logger.DebugFormatted("✅ Successfully transferred and loaded image {} on {}", image, worker.IP);
        } });
    for (const auto &worker : AtlasNet::Get().GetSettings().workers)
    {
    }

    logger.Debug("🧹 All images processed for all workers.");
}

void AtlasNetBootstrap::SetupPartitionService()
{

    const std::string registry = ManagerPcAdvertiseAddr + ":5000";

    logger.Debug("Cleaning up existing partition services");
    std::string CleanupPartitionServiceCommand = std::string("docker service rm ") + PartitionServiceName + "> /dev/null 2>&1";
    int deleteResult = system(CleanupPartitionServiceCommand.c_str());

    std::string imageTag = RunningLocally ? PartitionImageName : std::format("{}/{}:latest", registry, PartitionImageName);
    logger.DebugFormatted("Creating partition service from public image '{}'", imageTag);
    std::string CreatePartitionServiceCmd = std::string("docker service create --name ") + PartitionServiceName + " --network " + NetworkName + " --mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock --replicas 0 --detach=true " + imageTag;
    int createResult = system(CreatePartitionServiceCmd.c_str());
    if (createResult != 0)
        logger.ErrorFormatted("Failed to create partition service. Error Code {}", createResult);
    else
        logger.DebugFormatted("Docker Swarm started with service 'partition' (0 replicas).");
}

void AtlasNetBootstrap::SetupDemigodService()
{
    const std::string registry = ManagerPcAdvertiseAddr + ":5000";

    logger.Debug("Cleaning up existing demigod services");

    std::string RemoveCmd =
        std::string("docker service rm ") + DemigodServiceName + " > /dev/null 2>&1";
    system(RemoveCmd.c_str());

    // Determine correct image reference
    std::string imageTag = RunningLocally ? DemigodImageName : std::format("{}/{}:latest", registry, DemigodImageName);

    logger.DebugFormatted("Creating demigod service from image '{}'", imageTag);

    // Publish a random port, same logic as God uses
    //      -p :<port> publishes on a RANDOM ephemeral host port
    //        Docker Swarm will pick the host port
    std::string ServiceCmd =
        "docker service create "
        "--name " +
        DemigodServiceName + " "
                             "--network " +
        NetworkName + " "
                      "--mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock "
                      "--replicas 2 "
                      "--publish published=0,target=" +
        std::to_string(_PORT_DEMIGOD) + ",mode=ingress "
                                        "--detach=true " +
        imageTag;

    int result = system(ServiceCmd.c_str());
    if (result != 0)
    {
        logger.ErrorFormatted("Failed to create demigod service. Error code {}", result);
    }
    else
    {
        logger.DebugFormatted("Docker Swarm started demigod service '{}' (0 replicas).",
                              DemigodServiceName);
    }
}

void AtlasNetBootstrap::GenerateTSLCertificate()
{
    const std::string certPath = tlsPath + "/server.crt";
    const std::string keyPath = tlsPath + "/server.key";
    const std::string configPath = tlsPath + "/san.cnf";
    const std::string ip = ManagerPcAdvertiseAddr;

    // 🔹 Check if existing certificate matches the current IP
    bool needsRegeneration = false;
    if (std::filesystem::exists(certPath) && std::filesystem::exists(keyPath))
    {
        // Extract the IPs from the current certificate’s SAN
        std::string cmd = std::format("openssl x509 -in '{}' -noout -text | grep -A1 'Subject Alternative Name'", certPath);
        CommandResult res = RunCommand(cmd);

        if (res.exitCode == 0)
        {
            if (res.output.find(ip) == std::string::npos)
            {
                logger.WarningFormatted("⚠️ TLS certificate IP '{}' does not match current ManagerPcAdvertiseAddr '{}'. Regenerating...",
                                        res.output, ip);
                needsRegeneration = true;
            }
        }
        else
        {
            logger.Warning("⚠️ Failed to read existing certificate SAN. Regenerating...");
            needsRegeneration = true;
        }
    }

    // 🔹 Delete old certificates if regeneration is required
    if (needsRegeneration)
    {
        try
        {
            std::filesystem::remove(certPath);
            std::filesystem::remove(keyPath);
            logger.Debug("🗑 Old certificate and key deleted due to IP change");
        }
        catch (const std::exception &e)
        {
            logger.ErrorFormatted("❌ Failed to delete old certificate: {}", e.what());
        }
    }

    // 🔹 If still valid and IP hasn’t changed, return early
    if (!needsRegeneration && std::filesystem::exists(certPath) && std::filesystem::exists(keyPath))
    {
        logger.DebugFormatted("✅ Existing TLS certificate is valid for '{}'", ip);
        return;
    }

    // 🔹 Generate new certificate
    logger.DebugFormatted("🔒 Generating new self-signed TLS certificate with SANs for IP '{}'", ip);
    std::filesystem::create_directories(tlsPath);

    std::string hostname = RunCommand("hostname").output;
    hostname.erase(std::remove(hostname.begin(), hostname.end(), '\n'), hostname.end());

    // --- Write OpenSSL SAN config ---
    std::ofstream config(configPath);
    config << "[ req ]\n";
    config << "default_bits       = 4096\n";
    config << "distinguished_name = req_distinguished_name\n";
    config << "req_extensions     = v3_req\n";
    config << "prompt             = no\n\n";
    config << "[ req_distinguished_name ]\n";
    config << "CN = " << hostname << "\n\n";
    config << "[ v3_req ]\n";
    config << "subjectAltName = @alt_names\n\n";
    config << "[ alt_names ]\n";
    config << "DNS.1 = " << hostname << "\n";
    config << "DNS.2 = localhost\n";
    config << "IP.1 = " << ip << "\n";
    config.close();

    // --- Run OpenSSL ---
    std::string command = std::format(
        "openssl req -x509 -nodes -newkey rsa:4096 "
        "-keyout '{}' "
        "-out '{}' "
        "-days 3650 "
        "-config '{}' "
        "-extensions v3_req 2>&1",
        keyPath,
        certPath,
        configPath);

    int result = std::system(command.c_str());
    if (result != 0)
    {
        logger.Error("❌ Failed to generate TLS certificate with SANs");
        throw std::runtime_error("TLS certificate generation failed");
    }

    logger.DebugFormatted("✅ TLS certificate and key generated successfully with SANs (CN='{}', IP='{}')", hostname, ip);
}

void AtlasNetBootstrap::GetWorkersSSHCredentials()
{
    if (AtlasNet::Get().GetSettings().workers.empty())
    {
        logger.Debug("No workers specified. running of this machine");
        return;
    }

    std::filesystem::create_directories(KeysPath);

    const auto &workers = AtlasNet::Get().GetSettings().workers;
    std::for_each(workers.begin(), workers.end(), [&](const Worker &worker)
                  {
        std::string userFile = KeysPath + "/user_" + worker.Name + ".txt";
        std::replace(userFile.begin(), userFile.end(), ' ', '_');

        std::string sshUser;
        if (std::filesystem::exists(userFile))
        {
            std::ifstream in(userFile);
            std::getline(in, sshUser);
            in.close();
            logger.DebugFormatted("Using saved SSH username '{}' for worker {}", sshUser, worker.Name);
        }
        else
        {
            std::unique_lock lock(AskInputMutex);
            std::cout << "Enter SSH username for worker '" << worker.Name
                      << "' (" << worker.IP << ") [default: root]: ";
            if (std::cin.peek() == '\n')
                std::cin.ignore();
            std::getline(std::cin, sshUser);
            if (sshUser.empty())
                sshUser = "root";

            std::ofstream out(userFile);
            out << sshUser;
            out.close();
            logger.DebugFormatted("Saved SSH username '{}' for worker {}", sshUser, worker.Name);
        }

        std::string keyName = "id_rsa_" + worker.Name;
        std::replace(keyName.begin(), keyName.end(), ' ', '_');
        std::string privateKeyPath = KeysPath + "/" + keyName;
        std::string publicKeyPath = privateKeyPath + ".pub";
        if (!std::filesystem::exists(privateKeyPath))
        {
            std::string keygenCmd =
                "ssh-keygen -t rsa -b 4096 -f \"" + privateKeyPath + "\" -N '' -q";
            logger.DebugFormatted("Generating SSH key for {} ...", worker.Name);
            system(keygenCmd.c_str());
        }
        {
            std::unique_lock lock(AskInputMutex);
            logger.DebugFormatted("Copying SSH key to worker {} ({}) ...", worker.Name, worker.IP);
        std::string copyCmd =
            "ssh-copy-id -i " + publicKeyPath + " -o StrictHostKeyChecking=no " +
            sshUser + "@" +
            worker.IP + " > /dev/null 2>&1";
        auto copyResult = RunCommand(copyCmd.c_str());
        if (copyResult.exitCode != 0)
        {
            std::cerr << copyResult.output << std::endl;
            logger.ErrorFormatted("❌ Failed to copy SSH key to {}", worker.IP);
            return;
        }
        }
      
        std::string testCmd =
            "ssh -i " + privateKeyPath + " -o StrictHostKeyChecking=no " + sshUser + "@" +
            worker.IP + " echo 'SSH connection successful'";
        system(testCmd.c_str());

        OnlineWorker ow;
        ow.worker = worker;
        ow.sshUser = sshUser;
        ow.publicKeyPath = publicKeyPath;
        ow.PrivateKeyPath = privateKeyPath;
        static std::mutex pushVectorMutex;
        std::unique_lock lock(pushVectorMutex);
        onlineWorkers.push_back(ow); });
}
void AtlasNetBootstrap::SendTLSCertificateToWorkers()
{

    const std::string certPath = tlsPath + "/server.crt";
    const std::string keyPath = tlsPath + "/server.key";

    if (!std::filesystem::exists(certPath) || !std::filesystem::exists(keyPath))
    {
        logger.Error("❌ TLS certificate or key missing. Run EnsureTLSCertificate() first.");
        return;
    }

    // Compute local certificate hash
    auto hashRes = RunCommand(std::format("sha256sum {} | awk '{{print $1}}'", certPath));
    if (hashRes.exitCode != 0 || hashRes.output.empty())
    {
        logger.Error("❌ Failed to compute local certificate hash");
        return;
    }

    std::string localHash = hashRes.output;
    localHash.erase(std::remove(localHash.begin(), localHash.end(), '\n'), localHash.end());

    const std::string managerHost = ManagerPcAdvertiseAddr;
    const std::string certInstallPath = std::format("/etc/docker/certs.d/{}:{}/ca.crt", managerHost, registryPort);

    // === Apply certificate locally ===
    {
        logger.DebugFormatted("🖥️ Checking TLS certificate on local machine for registry {}:{}", managerHost, registryPort);

        auto checkRes = RunCommand(std::format("[ -f {} ] && sha256sum {} | awk '{{print $1}}' || echo MISSING", certInstallPath, certInstallPath));
        std::string currentLocalHash = checkRes.output;
        currentLocalHash.erase(std::remove(currentLocalHash.begin(), currentLocalHash.end(), '\n'), currentLocalHash.end());

        if (currentLocalHash == localHash)
        {
            logger.Debug("🔁 Local machine already has the same TLS certificate — skipping");
        }
        else
        {
            logger.Debug("📥 Installing TLS certificate on local machine");
            std::cout << "Enter sudo password for local machine: ";
            std::string password = GetHiddenInput("");

            std::string localInstallCmd = std::format(
                "echo '{}' | sudo -S -p '' mkdir -p /etc/docker/certs.d/{}:{} && "
                "echo '{}' | sudo -S -p '' cp {} {} && "
                "echo '{}' | sudo -S -p '' systemctl restart docker",
                password, managerHost, registryPort,
                password, certPath, certInstallPath,
                password);

            auto localRes = RunCommand(localInstallCmd);
            if (localRes.exitCode == 0)
                logger.Debug("✅ TLS certificate installed on local machine");
            else
                logger.ErrorFormatted("❌ Failed to install TLS certificate on local machine:\n{}", localRes.output);
        }
    }

    std::for_each(std::execution::par_unseq, onlineWorkers.begin(), onlineWorkers.end(), [&](const OnlineWorker &onlineworker)
                  {
logger.DebugFormatted("📡 Checking TLS certificate for worker {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);

        const std::string remoteCertPath = std::format("/etc/docker/certs.d/{}:{}/ca.crt", managerHost, registryPort);

        // Compute hash on worker
        std::string remoteHashCmd = std::format(
    "ssh -i '{}' -o StrictHostKeyChecking=no {}@{} "
    "'[ -f \"{}\" ] && sha256sum \"{}\" | awk \"{{print $1}}\" || echo MISSING'",
    onlineworker.PrivateKeyPath,
    onlineworker.sshUser,
    onlineworker.worker.IP,
    remoteCertPath,
    remoteCertPath);

        auto remoteHashRes = RunCommand(remoteHashCmd);
        std::string remoteHash = remoteHashRes.output;
        remoteHash.erase(std::remove(remoteHash.begin(), remoteHash.end(), '\n'), remoteHash.end());
        if (auto spaceLoc = remoteHash.find(' ');spaceLoc != remoteHash.npos)
        {
            remoteHash = remoteHash.substr(0,spaceLoc);
        }
        remoteHash.erase(std::remove(remoteHash.begin(), remoteHash.end(), ' '), remoteHash.end());

        if (remoteHash == localHash)
        {
            logger.DebugFormatted("🔁 {}@{} already has the same TLS certificate — skipping", onlineworker.worker.Name, onlineworker.worker.IP);
            return;
        }

        logger.DebugFormatted("📤 Sending TLS certificate to worker {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);

        
        // Send the file to a temp directory
        std::string scpCmd = std::format(
            "scp -i \"{}\" -o StrictHostKeyChecking=no {} {}@{}:~/atlasnet_certs_tmp/ca.crt",
            onlineworker.PrivateKeyPath, certPath, onlineworker.sshUser, onlineworker.worker.IP);
        RunCommand(std::format("ssh -i \"{}\" -o StrictHostKeyChecking=no {}@{} \"mkdir -p ~/atlasnet_certs_tmp\"",
                               onlineworker.PrivateKeyPath, onlineworker.sshUser, onlineworker.worker.IP));

        auto scpRes = RunCommand(scpCmd);
        if (scpRes.exitCode != 0)
        {
            logger.ErrorFormatted("❌ Failed to copy TLS certificate to {}@{}:\n{}", onlineworker.worker.Name, onlineworker.worker.IP, scpRes.output);
            return;
        }
        std::string password;
        {
            std::lock_guard guard(AskInputMutex);
             std::cout << "Enter sudo password for " << onlineworker.sshUser
                  << "@" << onlineworker.worker.IP << ": ";
                password = GetHiddenInput("");
        }
       

        std::string trustCmd = std::format(
            "ssh -i \"{}\" -o StrictHostKeyChecking=no {}@{} "
            "\"echo '{}' | sudo -S mkdir -p /etc/docker/certs.d/{}:{} && "
            "echo '{}' | sudo -S cp ~/atlasnet_certs_tmp/ca.crt /etc/docker/certs.d/{}:{}/ca.crt && "
            "echo '{}' | sudo -S systemctl restart docker\"",
            onlineworker.PrivateKeyPath,
            onlineworker.sshUser,
            onlineworker.worker.IP,
            password,
            managerHost, registryPort,
            password,
            managerHost, registryPort,
            password);

        auto trustRes = RunCommand(trustCmd);
        if (trustRes.exitCode == 0)
            logger.DebugFormatted("✅ TLS certificate installed on {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);
        else
            logger.ErrorFormatted("❌ Failed to install TLS certificate on {}@{}:\n{}", onlineworker.worker.Name, onlineworker.worker.IP, trustRes.output); });
    /*
    // === Apply cer;tificate to workers ===
    for (const auto &onlineworker : onlineWorkers)
    {
        logger.DebugFormatted("📡 Checking TLS certificate for worker {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);

        const std::string remoteCertPath = std::format("/etc/docker/certs.d/{}:{}/ca.crt", managerHost, registryPort);

        // Compute hash on worker
        std::string remoteHashCmd = std::format(
    "ssh -i '{}' -o StrictHostKeyChecking=no {}@{} "
    "'[ -f \"{}\" ] && sha256sum \"{}\" | awk \"{{print $1}}\" || echo MISSING'",
    onlineworker.PrivateKeyPath,
    onlineworker.sshUser,
    onlineworker.worker.IP,
    remoteCertPath,
    remoteCertPath);

        auto remoteHashRes = RunCommand(remoteHashCmd);
        std::string remoteHash = remoteHashRes.output;
        remoteHash.erase(std::remove(remoteHash.begin(), remoteHash.end(), '\n'), remoteHash.end());
        if (auto spaceLoc = remoteHash.find(' ');spaceLoc != remoteHash.npos)
        {
            remoteHash = remoteHash.substr(0,spaceLoc);
        }
        remoteHash.erase(std::remove(remoteHash.begin(), remoteHash.end(), ' '), remoteHash.end());

        if (remoteHash == localHash)
        {
            logger.DebugFormatted("🔁 {}@{} already has the same TLS certificate — skipping", onlineworker.worker.Name, onlineworker.worker.IP);
            continue;
        }

        logger.DebugFormatted("📤 Sending TLS certificate to worker {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);

        // Send the file to a temp directory
        std::string scpCmd = std::format(
            "scp -i \"{}\" -o StrictHostKeyChecking=no {} {}@{}:~/atlasnet_certs_tmp/ca.crt",
            onlineworker.PrivateKeyPath, certPath, onlineworker.sshUser, onlineworker.worker.IP);
        RunCommand(std::format("ssh -i \"{}\" -o StrictHostKeyChecking=no {}@{} \"mkdir -p ~/atlasnet_certs_tmp\"",
                               onlineworker.PrivateKeyPath, onlineworker.sshUser, onlineworker.worker.IP));

        auto scpRes = RunCommand(scpCmd);
        if (scpRes.exitCode != 0)
        {
            logger.ErrorFormatted("❌ Failed to copy TLS certificate to {}@{}:\n{}", onlineworker.worker.Name, onlineworker.worker.IP, scpRes.output);
            continue;
        }

        std::cout << "Enter sudo password for " << onlineworker.sshUser
                  << "@" << onlineworker.worker.IP << ": ";
        std::string password = GetHiddenInput("");

        std::string trustCmd = std::format(
            "ssh -i \"{}\" -o StrictHostKeyChecking=no {}@{} "
            "\"echo '{}' | sudo -S mkdir -p /etc/docker/certs.d/{}:{} && "
            "echo '{}' | sudo -S cp ~/atlasnet_certs_tmp/ca.crt /etc/docker/certs.d/{}:{}/ca.crt && "
            "echo '{}' | sudo -S systemctl restart docker\"",
            onlineworker.PrivateKeyPath,
            onlineworker.sshUser,
            onlineworker.worker.IP,
            password,
            managerHost, registryPort,
            password,
            managerHost, registryPort,
            password);

        auto trustRes = RunCommand(trustCmd);
        if (trustRes.exitCode == 0)
            logger.DebugFormatted("✅ TLS certificate installed on {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);
        else
            logger.ErrorFormatted("❌ Failed to install TLS certificate on {}@{}:\n{}", onlineworker.worker.Name, onlineworker.worker.IP, trustRes.output);
    }*/
}

void AtlasNetBootstrap::SetupRegistry()
{
    logger.Debug("🧱 Setting up Docker Registry service...");

    const std::string registryName = "registry";
    const int registryPort = 5000;

    // Remove any existing registry service
    std::string cleanupCmd = std::format("docker service rm {} > /dev/null 2>&1", registryName);
    system(cleanupCmd.c_str());

    // Create a volume for registry data if it doesn't exist
    std::string createVolumeCmd = "docker volume create registry_data > /dev/null 2>&1";
    int volumeResult = system(createVolumeCmd.c_str());
    if (volumeResult != 0)
    {
        logger.Error("❌ Failed to create registry volume.");
        return;
    }

    // Create the registry service connected to AtlasNet overlay
    std::string createServiceCmd = std::format(
        "docker service create "
        "--name {} "
        "--network {} "
        "-p {}:{} "
        "--mount type=volume,source=registry_data,target=/var/lib/registry "
        "--mount type=bind,source={}/keys/tls/server.crt,target=/certs/server.crt,readonly "
        "--mount type=bind,source={}/keys/tls/server.key,target=/certs/server.key,readonly "
        "--env REGISTRY_HTTP_ADDR=0.0.0.0:5000 "
        "--env REGISTRY_HTTP_TLS_CERTIFICATE=/certs/server.crt "
        "--env REGISTRY_HTTP_TLS_KEY=/certs/server.key "
        "--constraint node.role==manager "
        "registry:2",
        registryName,
        NetworkName,
        registryPort,
        registryPort,
        std::filesystem::current_path().string(), // or replace with your project root if needed
        std::filesystem::current_path().string());

    int createResult = system(createServiceCmd.c_str());
    if (createResult != 0)
    {
        logger.ErrorFormatted("❌ Failed to create registry service (error code {}).", createResult);
        return;
    }

    // Wait a moment for the service to stabilize
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify registry service is running
    std::string checkCmd = std::format(
        "docker service ls --filter name={} --format '{{{{.Name}}}}: {{{{.Replicas}}}}'", registryName);
    CommandResult result = RunCommand(checkCmd);
    if (result.exitCode == 0 && result.output.find("1/1") != std::string::npos)
    {
        logger.DebugFormatted("✅ Docker registry '{}' is running on port {} and connected to '{}'.",
                              registryName, registryPort, NetworkName);
    }
    else
    {
        logger.WarningFormatted("⚠️ Registry service '{}' may not be running yet:\n{}", registryName, result.output);
    }
}
void AtlasNetBootstrap::ClearOldCache()
{
    system("docker builder prune -a --filter=24h --force");
}
void AtlasNetBootstrap::MakeDockerBuilder()
{
    const std::string registryHost = ManagerPcAdvertiseAddr + ":5000";
    const std::string certBaseDir = "keys/tls";
    const std::string certPath = certBaseDir + "/server.crt";
    const std::string containerName = std::format("buildx_buildkit_{}0", ToLower(BuilderName));
    const std::string builderListCmd = std::format("docker buildx ls | grep -w {}", BuilderName);

    bool builderExists = (RunCommand(builderListCmd).exitCode == 0);
    bool certValid = false;
    bool certSame = false;

    // 🔹 Check if cert exists and is valid
    if (!RunningLocally && std::filesystem::exists(certPath))
    {
        auto validityCheck = RunCommand(std::format("openssl x509 -checkend 86400 -noout -in {}", certPath));
        certValid = (validityCheck.exitCode == 0);

        // 🔹 Compare fingerprint with what was last used
        const std::string fingerprintFile = certBaseDir + "/last_cert_fingerprint.txt";

        auto currentFingerprint = RunCommand(std::format("openssl x509 -noout -fingerprint -sha256 -in {}", certPath));
        if (currentFingerprint.exitCode == 0)
        {
            std::string currentFP = currentFingerprint.output;
            currentFP.erase(std::remove(currentFP.begin(), currentFP.end(), '\n'), currentFP.end());

            if (std::filesystem::exists(fingerprintFile))
            {
                std::ifstream in(fingerprintFile);
                std::string oldFP;
                std::getline(in, oldFP);

                certSame = (oldFP == currentFP);
            }

            // Update stored fingerprint (for future runs)
            std::ofstream out(fingerprintFile, std::ios::trunc);
            out << currentFP;
        }
    }

    // 🔹 If builder exists and certificate is valid and identical, skip
    if (builderExists && (RunningLocally || (certValid && certSame)))
    {
        logger.DebugFormatted(
            "✅ Builder '{}' already exists and certificate '{}' is still valid and unchanged. Skipping recreation.",
            BuilderName, certPath);
        return;
    }

    // 🔹 If builder exists but cert invalid/different, delete it
    if (builderExists)
    {
        logger.DebugFormatted("🧱 Builder '{}' exists but certificate invalid or changed. Recreating builder...", BuilderName);
        RunCommand(std::format("docker buildx rm -f {}", BuilderName));
    }

    // 🔹 Create builder
    logger.DebugFormatted("🧱 Creating builder '{}'", BuilderName);
    std::string createCmd = std::format(
        "docker buildx create "
        "--name {} "
        "--driver docker-container "
        "--use "
        "--driver-opt network=host "
        "--driver-opt memory={}g "
        "--buildkitd-flags '--allow-insecure-entitlement network.host'",
        BuilderName, AtlasNet::Get().GetSettings().BuilderMemoryGb.value_or(UINT32_MAX));

    logger.Debug(createCmd);
    const auto CreateResult = RunCommand(createCmd);
    if (CreateResult.exitCode != 0)
    {
        logger.ErrorFormatted("❌ Failed to create builder '{}'\n {}", BuilderName, CreateResult.output);
        return;
    }

    logger.DebugFormatted("✅ Builder '{}' created successfully", BuilderName);

    // 🔹 Bootstrap builder
    logger.DebugFormatted("🚀 Bootstrapping builder '{}'", BuilderName);
    if (RunCommand(std::format("docker buildx inspect {} --bootstrap", BuilderName)).exitCode != 0)
    {
        logger.ErrorFormatted("❌ Failed to bootstrap builder '{}'", BuilderName);
        return;
    }

    if (!RunningLocally)
    {
        // 🔹 Copy certificate into container
        logger.DebugFormatted("📦 Copying registry certificate '{}' into builder '{}'", certPath, containerName);
        RunCommand(std::format(
            "docker exec {} mkdir -p /usr/local/share/ca-certificates/{}",
            containerName, registryHost));

        std::string copyCmd = std::format(
            "docker cp {} {}:/usr/local/share/ca-certificates/{}/registry.crt",
            certPath, containerName, registryHost);
        const auto copyResult = RunCommand(copyCmd);
        if (copyResult.exitCode != 0)
        {
            std::cerr << copyResult.output << std::endl;
            logger.ErrorFormatted("❌ Failed to copy certificate into builder container '{}'", containerName);
            return;
        }

        logger.DebugFormatted("🔑 Installing registry certificate manually (BusyBox environment detected)");
        const auto manualAdd = RunCommand(std::format(
            "docker exec {} sh -c 'mkdir -p /etc/ssl/certs && cat /usr/local/share/ca-certificates/{}/registry.crt >> /etc/ssl/certs/ca-certificates.crt'",
            containerName, registryHost));
        std::cerr << manualAdd.output << std::endl;
        logger.DebugFormatted("✅ Registry certificate successfully installed in builder '{}'", containerName);
    }
}

/*
std::string AtlasNetBootstrap::EnsureDockerLogin()
{
    // Not logged in → ask credentials
    std::string username;
    std::cout << "🧑 Enter Docker username: ";
    if (std::cin.peek() == '\n')
        std::cin.ignore();
    std::getline(std::cin, username);
    std::string password = GetHiddenInput("🔑 Enter Docker password: ");

    // Perform secure login
    std::string cmd = std::format("echo '{}' | docker login --username '{}' --password-stdin", password, username);
    int result = std::system(cmd.c_str());
    if (result == 0)
    {
        std::cout << "✅ Successfully logged in as " << username << std::endl;
        dockerUserName = username;
        return username;
    }
    else
    {
        throw std::runtime_error("❌ Docker login failed");
    }
}
*/
std::string AtlasNetBootstrap::GetRegistrysIP()
{
    std::string serviceName = "registry";
    const std::string filter = "{\"service\":[\"" + serviceName + "\"]}";
    std::string tasksResponse = DockerIO::Get().request("GET", "/tasks?filters=" + filter);
    Json tasks = Json::parse(tasksResponse);

    if (tasks.empty())
        throw std::runtime_error("No tasks found for service '" + serviceName + "'");

    // 2️⃣ Find a running task that has a container ID
    std::string containerID;
    for (const auto &task : tasks)
    {
        if (task.contains("DesiredState") &&
            task["DesiredState"] == "running" &&
            task.contains("Status") &&
            task["Status"].contains("ContainerStatus") &&
            task["Status"]["ContainerStatus"].contains("ContainerID"))
        {
            containerID = task["Status"]["ContainerStatus"]["ContainerID"].get<std::string>();
            break;
        }
    }

    if (containerID.empty())
        throw std::runtime_error("Could not find a running container for service '" + serviceName + "'");

    // 3️⃣ Inspect that container to get its network IP
    std::string inspectResponse = DockerIO::Get().request("GET", "/containers/" + containerID + "/json");
    Json containerInfo = Json::parse(inspectResponse);

    if (!containerInfo.contains("NetworkSettings") || !containerInfo["NetworkSettings"].contains("Networks"))
        throw std::runtime_error("Invalid container info for " + containerID);

    for (auto &[netName, netData] : containerInfo["NetworkSettings"]["Networks"].items())
    {
        if (netData.contains("IPAddress"))
            return netData["IPAddress"].get<std::string>();
    }

    throw std::runtime_error("No IP found for container " + containerID);
}
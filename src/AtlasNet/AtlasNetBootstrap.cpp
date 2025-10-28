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
    SetupSwarm();
    SetupNetwork();
    GenerateTSLCertificate();
    SetupRegistry();

    // 2. SSH to workers. muist happen first so their arches can be retrieved
    GetWorkersSSHCredentials();
    SendTLSCertificateToWorkers();
    JoinWorkerToSwarm();

    CreateDevContainerJson();
    MakeDockerBuilder();
    CreateGodImage();
    CreatePartitionImage();
    CreateDatabaseImage();

    RunDatabase();
    SetupPartitionService();
    RunGod();
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
                logger.ErrorFormatted("macro {} not parsed", it->str());
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
                                                         {"EXECUTABLE", "God"}});
    OutputDockerFile("God/Dockerfile", DockerFileContents);
    BuildDockerImage("God/Dockerfile", GodImageName, AtlasNet::Get().GetSettings().RuntimeArches);
}

void AtlasNetBootstrap::CreatePartitionImage()
{
    CreateLaunchJson("Partition/launch.json", {"Partition", "UnitTests"});

    std::string DockerFileContents = DockerImageBase + BuildBinariesInstructions;
    std::string packages;
    for (const auto &p : RequiredPackages)
    {
        packages += p + " ";
    }
    if (const std::string GameServerFiles = AtlasNet::Get().GetSettings().GameServerFiles; !GameServerFiles.empty())
    {
        DockerFileContents += "COPY " + GameServerFiles + " " + WorkDir + "\n";
    }
    const std::string EntryPointCmd = MacroParse(R"(
        RUN cat <<'EOF' > /entrypoint.sh
#!/bin/bash
set -euo pipefail

${exe1} &
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

RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/usr/bin/tini", "--", "/entrypoint.sh"])",
                                                 {{"exe1", "bin/" + BuildConfig + "/Partition/Partition"}, {"exe2", AtlasNet::Get().GetSettings().GameServerBinary}});
    DockerFileContents += EntryPointCmd;
    DockerFileContents = MacroParse(DockerFileContents, {{"OS_VERSION", OS_Version},
                                                         {"BUILD_CONFIG", ToLower(BuildConfig)},
                                                         {"WORKDIR", WorkDir},
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
                                                         {"DEV_PACKAGES", packages},
                                                         {"EXECUTABLE", "Database"}});
    OutputDockerFile("Database/Dockerfile", DockerFileContents);
    BuildDockerImage("Database/Dockerfile", DatabaseImageName, AtlasNet::Get().GetSettings().RuntimeArches);
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

        std::string tag = std::format("{}:{}", ImageName, archn);
        std::string fullTag = std::format("{}/{}", registry, tag);
        fullTags.push_back(fullTag);

        // Build the image
        std::string buildCmd = std::format(
            "docker buildx build --builder {} "
            "--platform linux/{} "
            "-f {}/{} -t {} "
            "--cache-from type=local,src={} "
            "--cache-to type=local,dest={},mode=max --push .",
            BuilderName, archn, DockerFilesFolder, DockerFile, fullTag, BuilderCacheDir, BuilderCacheDir);

        logger.DebugFormatted("🏗️ Building image '{}' for arch '{}' using '{}'", ImageName, archn, DockerFile);
        if (std::system(buildCmd.c_str()) != 0)
        {
            logger.ErrorFormatted("❌ Failed to build image '{}' for arch '{}'", ImageName, archn);
            throw std::runtime_error("Docker build failed");
        }

        logger.DebugFormatted("✅ Successfully built & pushed '{}' (arch '{}')", ImageName, archn);
    }
    // 2️⃣ Create and push manifest list (multi-arch tag)
    std::string manifestName = std::format("{}/{}:latest", registry, ImageName);
    std::string manifestCmd = std::format("docker manifest create {}", manifestName);

    for (const auto &fullTag : fullTags)
        manifestCmd += std::format(" {}", fullTag);

    logger.DebugFormatted("🧩 Creating manifest '{}'", manifestName);

    if (std::system(manifestCmd.c_str()) != 0)
    {
        logger.ErrorFormatted("❌ Failed to create manifest '{}'", manifestName);
        throw std::runtime_error("Manifest creation failed");
    }

    // 3️⃣ Annotate each arch in the manifest
    for (const auto &arch : arches)
    {
        std::string archn = arch;
        archn.erase(std::remove_if(archn.begin(), archn.end(), ::isspace), archn.end());
        std::string fullTag = std::format("{}/{}:{}", registry, ImageName, archn);

        std::string annotateCmd = std::format(
            "docker manifest annotate {} {} --os linux --arch {}", manifestName, fullTag, archn);
        std::system(annotateCmd.c_str());
    }

    // 4️⃣ Push manifest
    logger.DebugFormatted("🚀 Pushing manifest '{}'", manifestName);
    if (std::system(std::format("docker manifest push {}", manifestName).c_str()) != 0)
    {
        logger.ErrorFormatted("❌ Failed to push manifest '{}'", manifestName);
        throw std::runtime_error("Manifest push failed");
    }

    logger.DebugFormatted("✅ Multi-arch manifest pushed successfully: '{}'", manifestName);
    return true;
}

void AtlasNetBootstrap::RunGod()
{
    std::string removeCommand = "docker rm -f God > /dev/null 2>&1";
    system(removeCommand.c_str());

    logger.Debug("Running God");
    std::string Command = "docker run --init  --stop-timeout=999999 --network " + NetworkName + " -v /var/run/docker.sock:/var/run/docker.sock --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name God -d -p :" + std::to_string(_PORT_GOD) + " "+ ManagerPcAdvertiseAddr+":5000/" + GodImageName;
    system(Command.c_str());
}

void AtlasNetBootstrap::RunDatabase()
{
    logger.Debug("Running Database");
    system("docker rm -f Database");

    // Create the container
    std::string runCommand =
        "docker create --name Database --network " + NetworkName + " -v redis_data:/data -p 6379:6379 " +ManagerPcAdvertiseAddr+":5000/"+ DatabaseImageName + " /bin/bash -c \"redis-server --appendonly yes & ./Database\"";
    system(runCommand.c_str());

    // Start it
    system("docker start Database");
}

void AtlasNetBootstrap::SetupSwarm()
{
    int leaveResult = system("docker swarm leave --force");
    if (leaveResult == 0)
        logger.Warning("Leaving existing docker swarm");

    logger.Debug("Initializing Docker Swarm");
    Json swarmInit{{"ListenAddr", "0.0.0.0:2377"}};
    std::string initResult = DockerIO::Get().request("POST", "/swarm/init", &swarmInit);

    auto parsed = Json::parse(initResult);
    if (parsed.contains("message") &&
        parsed["message"].get<std::string>().find("could not choose an IP address") != std::string::npos)
    {
        logger.Warning("⚠️ Docker swarm could not choose an IP address automatically.");

        // --- List interfaces and IPs ---
        std::vector<std::pair<std::string, std::string>> interfaces;
        struct ifaddrs *ifaddr;
        if (getifaddrs(&ifaddr) == -1)
        {
            perror("getifaddrs");
            return;
        }

        for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            char addr[INET_ADDRSTRLEN];
            void *in_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, in_addr, addr, sizeof(addr));
            interfaces.emplace_back(ifa->ifa_name, addr);
        }
        freeifaddrs(ifaddr);

        if (interfaces.empty())
        {
            logger.Error("No IPv4 interfaces found!");
            return;
        }

        // --- Ask user to select one ---
        std::cout << "\nAvailable network interfaces:\n";
        for (size_t i = 0; i < interfaces.size(); ++i)
        {
            std::cout << "  [" << i << "] " << interfaces[i].first
                      << " -> " << interfaces[i].second << "\n";
        }

        std::cout << "Select interface number to advertise: ";
        size_t choice = 0;
        std::cin >> choice;

        if (choice >= interfaces.size())
        {
            logger.Error("Invalid choice");
            return;
        }

        std::string chosenIP = interfaces[choice].second;
        logger.DebugFormatted("Selected interface {} ({})", interfaces[choice].first, chosenIP);

        // --- Retry swarm init with chosen IP ---
        Json swarmInitRetry{
            {"ListenAddr", "0.0.0.0:2377"},
            {"AdvertiseAddr", chosenIP + ":2377"}};

        initResult = DockerIO::Get().request("POST", "/swarm/init", &swarmInitRetry);
    }
    else
    {
        logger.DebugFormatted("Swarm init response:\n{}", parsed.dump(4));
    }

    std::string infoResp = DockerIO::Get().request("GET", "/info");
    auto infoJson = Json::parse(infoResp);
    ManagerPcAdvertiseAddr = infoJson["Swarm"]["NodeAddr"];
}

void AtlasNetBootstrap::SetupNetwork()
{
    // Delete existing network if it exists
    system("docker network rm AtlasNet > /dev/null 2>&1");
    std::string NetworkCommand = "docker network create --driver overlay --attachable " + NetworkName;
    int netResult = system(NetworkCommand.c_str());
    // 256 = network already exists
    if (netResult != 0 && netResult != 256)
    {
        logger.ErrorFormatted("Failed to create network. Error code {}", netResult);
        throw std::runtime_error("Failed to create network");
    }
    else
    {
        logger.DebugFormatted("✅ AtlasNet Overlay network created");
    }
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

    std::string imageTag = std::format("{}/{}:latest", registry, PartitionImageName);
    logger.DebugFormatted("Creating partition service from public image '{}'", imageTag);
    std::string CreatePartitionServiceCmd = std::string("docker service create --name ") + PartitionServiceName + " --network " + NetworkName + " --mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock --replicas 0 " + imageTag;
    int createResult = system(CreatePartitionServiceCmd.c_str());
    if (createResult != 0)
        logger.ErrorFormatted("Failed to create partition service. Error Code {}", createResult);
    else
        logger.DebugFormatted("Docker Swarm started with service 'partition' (0 replicas).");
}

void AtlasNetBootstrap::GenerateTSLCertificate()
{
    const std::string certPath = tlsPath + "/server.crt";
    const std::string keyPath = tlsPath + "/server.key";
    const std::string configPath = tlsPath + "/san.cnf";

    if (std::filesystem::exists(certPath) && std::filesystem::exists(keyPath))
    {
        logger.DebugFormatted("✅ Existing TLS certificate found at '{}'", tlsPath);
        return;
    }

    logger.DebugFormatted("🔒 Generating new self-signed TLS certificate with SANs at '{}'", tlsPath);
    std::filesystem::create_directories(tlsPath);

    // Detect hostname and IP address
    std::string hostname = RunCommand("hostname").output;
    hostname.erase(std::remove(hostname.begin(), hostname.end(), '\n'), hostname.end());
    std::string ip = RunCommand("hostname -I | awk '{print $1}'").output; // get first IP
    ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());

    // Generate OpenSSL SAN config
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

    // Generate certificate with SANs
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

    logger.DebugFormatted("✅ TLS certificate and key generated successfully with SANs (CN='{}', IP='{}')",
                          hostname, ip);
}

void AtlasNetBootstrap::GetWorkersSSHCredentials()
{
    if (AtlasNet::Get().GetSettings().workers.empty())
    {
        logger.Debug("No workers specified. running of this machine");
        return;
    }

    std::filesystem::create_directories(KeysPath);

    for (const auto &worker : AtlasNet::Get().GetSettings().workers)
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
        logger.DebugFormatted("Copying SSH key to worker {} ({}) ...", worker.Name, worker.IP);
        std::string copyCmd =
            "ssh-copy-id -i " + publicKeyPath + " -o StrictHostKeyChecking=no " +
            sshUser + "@" +
            worker.IP + " > /dev/null 2>&1";
        auto copyResult = RunCommand(copyCmd.c_str());
        if (copyResult.exitCode != 0)
        {   std::cerr << copyResult.output << std::endl;
            logger.ErrorFormatted("❌ Failed to copy SSH key to {}", worker.IP);
            continue;
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
        onlineWorkers.push_back(ow);
    }
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

    // === Apply certificate to workers ===
    for (const auto &onlineworker : onlineWorkers)
    {
        logger.DebugFormatted("📡 Checking TLS certificate for worker {}@{}", onlineworker.worker.Name, onlineworker.worker.IP);

        const std::string remoteCertPath = std::format("/etc/docker/certs.d/{}:{}/ca.crt", managerHost, registryPort);

        // Compute hash on worker
        std::string remoteHashCmd = std::format(
            "ssh -i \"{}\" -o StrictHostKeyChecking=no {}@{} "
            "\"[ -f '{}' ] && sha256sum '{}' | awk '{{print $1}}' || echo MISSING\"",
            onlineworker.PrivateKeyPath,
            onlineworker.sshUser,
            onlineworker.worker.IP,
            remoteCertPath,
            remoteCertPath);

        auto remoteHashRes = RunCommand(remoteHashCmd);
        std::string remoteHash = remoteHashRes.output;
        remoteHash.erase(std::remove(remoteHash.begin(), remoteHash.end(), '\n'), remoteHash.end());

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
    }
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
/*
void AtlasNetBootstrap::MakeDockerBuilder()
{
    std::filesystem::create_directories(BuilderCacheDir);

    std::string registryHost = ManagerPcAdvertiseAddr + ":5000";
    std::string localCertDir = std::format("keys/tls/{}", registryHost);
    std::filesystem::create_directories(localCertDir);

    // Ensure the correct file exists as ca.crt
    std::filesystem::copy_file("keys/tls/server.crt", localCertDir + "/ca.crt",
                               std::filesystem::copy_options::overwrite_existing);

    // 1️⃣ Remove old builder if it exists
    auto check = RunCommand(std::format("docker buildx ls | grep -w {}", BuilderName));
    if (check.exitCode == 0)
    {
        logger.DebugFormatted("⚙️ Builder {} already exists. Deleting it...", BuilderName);
        auto remove = RunCommand(std::format("docker buildx rm -f {}", BuilderName));
        if (remove.exitCode != 0)
        {
            logger.ErrorFormatted("❌ Failed to remove existing builder {}:\n{}", BuilderName, remove.output);
            throw std::runtime_error("Failed to remove existing builder");
        }
        logger.DebugFormatted("🗑️ Removed existing builder {}", BuilderName);
    }

    logger.DebugFormatted("🧱 Creating new builder {} (cache stored in '{}')", BuilderName, BuilderCacheDir);

    // 3️⃣ Create builder (remove invalid driver options)
    std::string createCmd = std::format(
        "docker buildx create "
        "--name {} "
        "--driver docker-container "
        "--use "
        "--driver-opt network=host "
        "--driver-opt image=moby/buildkit:latest "
        "--driver-opt \"mount={}:/etc/docker/certs.d/{}\"",
        BuilderName,
        std::filesystem::absolute("keys/tls").string(),
        registryHost);
    auto create = RunCommand(createCmd);
    if (create.exitCode != 0)
    {
        logger.ErrorFormatted("❌ Failed to create builder '{}':\n{}", BuilderName, create.output);
        throw std::runtime_error("Failed to create builder");
    }
    /*
        // 🕐 Trigger builder startup
        logger.DebugFormatted("🚀 Bootstrapping builder '{}' (starting BuildKit container)...", BuilderName);
        auto bootstrap = RunCommand(std::format("docker buildx inspect {} --bootstrap", BuilderName));
        if (bootstrap.exitCode != 0)
            logger.WarningFormatted("⚠️ Bootstrap returned non-zero (may still succeed):\n{}", bootstrap.output);

        // 🧩 Wait for builder container to appear
        std::string builderContainerName;
        for (int i = 0; i < 30; ++i) // up to ~6 seconds (30 * 200ms)
        {
            auto ps = RunCommand(
                "docker ps -a --filter 'name=builder' --format '{{.Names}}' | head -n1");
            builderContainerName = ps.output;
            builderContainerName.erase(
                std::remove(builderContainerName.begin(), builderContainerName.end(), '\n'),
                builderContainerName.end());

            if (!builderContainerName.empty())
            {
                logger.DebugFormatted("✅ Builder container '{}' is now running", builderContainerName);
                break;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }

        if (builderContainerName.empty())
        {
            logger.ErrorFormatted("❌ Timed out waiting for builder container '{}' to appear", BuilderName);
            throw std::runtime_error("Builder container did not start in time");
        }

        // builderContainerName.erase(std::find(builderContainerName.begin(), builderContainerName.end(), '\n'));

        if (builderContainerName.empty())
        {
            logger.WarningFormatted("⚠️ Could not locate builder container for '{}'", BuilderName);
        }
        else if (hasCert)
        {
            // 6️⃣ Copy TLS cert into the builder container
            logger.DebugFormatted("📦 Copying TLS cert '{}' into builder container '{}'", caCertPath, builderContainerName);

            RunCommand(std::format("docker exec {} mkdir -p {}", builderContainerName, certsDirInDocker));
            auto copy = RunCommand(std::format(
                "docker cp {} {}:{}", caCertPath, builderContainerName, certsDirInDocker + "/ca.crt"));

            if (copy.exitCode == 0)
            {
                logger.DebugFormatted("✅ Installed TLS cert into builder container at '{}'", certsDirInDocker);
                system(std::format("docker restart {}", builderContainerName).c_str());
            }
            else
                logger.WarningFormatted("⚠️ Failed to copy TLS cert into builder container:\n{}", copy.output);
        }


    // 7️⃣ Verify
    auto inspect = RunCommand(std::format("docker buildx inspect {}", BuilderName));
    if (inspect.exitCode == 0)
        logger.DebugFormatted("✅ Builder '{}' ready for use. Registry trust configured for '{}'", BuilderName, registryHost);
    else
        logger.WarningFormatted("⚠️ Builder '{}' created but inspection failed:\n{}", BuilderName, inspect.output);
}*/
void AtlasNetBootstrap::MakeDockerBuilder()
{
    const std::string registryHost = ManagerPcAdvertiseAddr + ":5000";
    const std::string certBaseDir = "keys/tls";
    const std::string certPath = certBaseDir + "/server.crt";
    // buildx names them like mybuilder0

    // 🔹 Remove old builder if exists
    if (RunCommand(std::format("docker buildx ls | grep -w {}", BuilderName)).exitCode == 0)
    {
        logger.DebugFormatted("🗑 Removing existing builder '{}'", BuilderName);
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
        "--buildkitd-flags '--allow-insecure-entitlement network.host'",
        BuilderName);

    if (RunCommand(createCmd).exitCode != 0)
    {
        logger.ErrorFormatted("❌ Failed to create builder '{}'", BuilderName);
        return;
    }

    logger.DebugFormatted("✅ Builder '{}' created successfully", BuilderName);

    logger.DebugFormatted("🚀 Bootstrapping builder '{}'", BuilderName);
    if (RunCommand(std::format("docker buildx inspect {} --bootstrap", BuilderName)).exitCode != 0)
    {
        logger.ErrorFormatted("❌ Failed to bootstrap builder '{}'", BuilderName);
        return;
    }
    // 🔹 Get container name (should be buildx_buildkit_<buildername>0)
    std::string containerName = std::format("buildx_buildkit_{}0", ToLower(BuilderName));

    // 🔹 Copy certificate into container's CA directory
    logger.DebugFormatted("📦 Copying registry certificate '{}' into builder '{}'", certPath, containerName);

    // Create temp path in container
    RunCommand(std::format(
        "docker exec {} mkdir -p /usr/local/share/ca-certificates/{}",
        containerName, registryHost));

    // Copy certificate into container
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
#include "DockerIO.hpp"

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}
std::string DockerIO::request(const std::string &method, const std::string &endpoint, const nlohmann::json *body, const std::vector<std::string> &extraHeaders) const
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to init curl");
    }
    std::string response;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    for (auto &h : extraHeaders)
    {
        headers = curl_slist_append(headers, h.c_str());
    }

    std::string url = "http://localhost" + endpoint;

    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, unixSocket.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    std::string bodyStr;
    if (body)
    {
        bodyStr = body->dump();
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, bodyStr.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    if (res != CURLE_OK)
    {
        headers = nullptr;
        curl_easy_cleanup(curl);
        throw std::runtime_error(std::string("curl_easy_perform failed: ") + curl_easy_strerror(res));
    }
    curl_easy_cleanup(curl);
    return response;
}

std::string DockerIO::InspectContainer(const DockerContainerID &ContainerID) const
{
    return request("GET", std::string("/containers/").append(ContainerID).append("/json"));
}

nlohmann::json DockerIO::InspectSelf() const
{
    if (SelfJson.has_value())
        return SelfJson.value();
    std::ifstream file("/etc/hostname");
    std::string id;
    if (file)
        std::getline(file, id);
    if (id.empty())
        throw std::runtime_error("Could not read /etc/hostname for container ID");

    std::string endpoint = "/containers/" + id + "/json";
    std::string response = request("GET", endpoint, nullptr, {});
    SelfJson.emplace(nlohmann::json::parse(response));
    return *SelfJson;
}

std::string DockerIO::GetSelfContainerName() const
{

    nlohmann::json info = InspectSelf();
    std::string name = info.value("Name", "");
    if (!name.empty() && name[0] == '/')
        name.erase(0, 1); // remove leading slash
    return name;
}

std::string DockerIO::GetSelfContainerIP() const
{
    nlohmann::json info = InspectSelf();
    std::string ip = info["NetworkSettings"]["Networks"]["AtlasNet"]["IPAddress"].get<std::string>();
    return ip;
}

std::vector<std::pair<uint32, uint32>> DockerIO::GetSelfExposedPorts() const
{
    std::vector<std::pair<uint32, uint32>> portBindings;
    const auto ports = InspectSelf()["NetworkSettings"]["Ports"];
    const auto portInfo = ports.items();
    for (const auto &[key, value] : portInfo)
    {
        const uint32 InternalPort = std::stoi(key.substr(0, key.find('/'))),
                     ExternalPort = std::stoi(value[0]["HostPort"].get<std::string>());
        portBindings.push_back({InternalPort, ExternalPort});
    }
    return portBindings;
}

std::optional<uint32> DockerIO::GetSelfExposedPortForInternalBind(uint32 InternalPort) const
{
    const auto ports = GetSelfExposedPorts();
    std::cerr << "Looking for "<< InternalPort<<std::endl;
    const auto find = std::find_if(ports.begin(), ports.end(), [InternalPort = InternalPort](std::pair<uint32, uint32> p)
                                   { std::cerr << p.first << std::endl; return InternalPort == p.first; });
    if (find == ports.end())
        return std::nullopt;

    return find->second;
}

#include "DockerIO.hpp"
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}
std::string DockerIO::request(const std::string &method, const std::string &endpoint, const nlohmann::json *body, const std::vector<std::string> &extraHeaders)
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

std::string DockerIO::InspectContainer(const DockerContainerID& ContainerID)
{
    return request("GET", std::string("/containers/").append(ContainerID).append("/json"));
}

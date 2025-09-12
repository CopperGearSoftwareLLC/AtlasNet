#include "God.hpp"
#include "pch.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include "curl/curl.h"

God::God() {}
God::~God()
{
    cleanupPartitions();
}

#include <curl/curl.h>
#include <iostream>
#include <string>

bool God::spawnPartition(int32_t id, int32_t port) 
{
    // ---------------------------
    // 1. Build JSON payload manually
    // ---------------------------
    // This tells Docker:
    //  - Which image to run ("partition_server")
    //  - Environment variable PARTITION_ID
    //  - Port mapping: map host:<port> to container:7777
    std::string payload = R"({
        "Image": "partition_server",
        "Env": [ "PARTITION_ID=)" + std::to_string(id) + R"(" ],
        "HostConfig": {
            "PortBindings": {
                "7777/tcp": [ { "HostPort": ")" + std::to_string(port) + R"(" } ]
            }
        }
    })";

    // ---------------------------
    // 2. Create container via Docker API
    // ---------------------------
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    // URL for creating container, with a custom name "partition_<id>"
    std::string url = "http://localhost/containers/create?name=partition_" + std::to_string(id);

    // Tell curl to talk to Docker via the Unix socket
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

    // Add JSON header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Capture the response into a string
    std::string response;
    auto writeCallback = [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        std::string* resp = static_cast<std::string*>(userdata);
        resp->append(ptr, size * nmemb);
        return size * nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Perform the API call
    CURLcode res = curl_easy_perform(curl);

    // Clean up request objects
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // Check for network/API errors
    if (res != CURLE_OK) {
        std::cerr << "Curl error: " << curl_easy_strerror(res) << "\n";
        return false;
    }

    // ---------------------------
    // 3. Parse container ID from response
    // ---------------------------
    // Docker returns JSON like: { "Id": "<containerId>", "Warnings": null }
    // Here we naively extract the "Id" string.
    size_t idPos = response.find("\"Id\":\"");
    if (idPos == std::string::npos) {
        std::cerr << "Failed to parse container ID: " << response << "\n";
        return false;
    }
    size_t start = idPos + 6; // skip past `"Id":"`
    size_t end = response.find("\"", start);
    std::string containerId = response.substr(start, end - start);

    // ---------------------------
    // 4. Start the container
    // ---------------------------
    CURL* curlStart = curl_easy_init();
    std::string startUrl = "http://localhost/containers/" + containerId + "/start";

    curl_easy_setopt(curlStart, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
    curl_easy_setopt(curlStart, CURLOPT_URL, startUrl.c_str());
    curl_easy_setopt(curlStart, CURLOPT_POST, 1L);

    // Run the "start" API call
    res = curl_easy_perform(curlStart);
    curl_easy_cleanup(curlStart);

    // If the start failed, report error
    if (res != CURLE_OK) {
        std::cerr << "Failed to start container " << containerId << ": " 
                  << curl_easy_strerror(res) << "\n";
        return false;
    }

    // ---------------------------
    // 5. Success log
    // ---------------------------
    std::cout << "Partition " << id << " spawned on port " << port 
              << " (container " << containerId.substr(0,12) << ")\n";

    return true;
}

bool God::cleanupPartitions() 
{
    int result = std::system("docker rm -f $(docker ps -aq --filter name=partition_)");
    if (result != 0) {
        std::cerr << "Failed to clean up partitions\n";
        return false;
    }
    return true;
}

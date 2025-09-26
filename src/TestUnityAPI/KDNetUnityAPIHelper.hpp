#pragma once
#include <cstdio>
#include <string>
#include <sstream>
#include <curl/curl.h>   // requires libcurl

// Return codes
enum KDNetStatus {
    KDNET_OK = 0,
    KDNET_ERR_NULL_ID = 1,
    KDNET_ERR_HTTP_FAIL = 2
};

// Helper: send JSON via HTTP POST
int send_entity_to_server(const std::string& json, const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl) return KDNET_ERR_HTTP_FAIL;

    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json.size());

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? KDNET_OK : KDNET_ERR_HTTP_FAIL;
}
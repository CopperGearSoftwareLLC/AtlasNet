// Build: Windows uses WinHTTP; Linux/macOS use libcurl.

#include <cstdio>
#include <string>
#include <sstream>

#if defined(_WIN32)
  #include <windows.h>
  #include <winhttp.h>
  #pragma comment(lib, "winhttp.lib")
#else
  #include <curl/curl.h>
#endif

enum KDNetStatus { KDNET_OK=0, KDNET_ERR_NULL_ID=1, KDNET_ERR_HTTP_FAIL=2 };

// ---------- Cross-platform HTTP POST JSON ----------
static int send_entity_to_server(const std::string& json, const std::string& url)
{
#if defined(_WIN32)
    // URL should be like: http://localhost:5000/entity
    // Parse scheme + host + path (quick & dirty)
    std::string scheme, host, path; int port = 80;
    {
        // very naive parser
        if (url.rfind("http://", 0) == 0) {
            scheme = "http"; port = 80;
            auto rest = url.substr(7);
            auto slash = rest.find('/');
            host = (slash==std::string::npos) ? rest : rest.substr(0, slash);
            path = (slash==std::string::npos) ? "/" : rest.substr(slash);
        } else if (url.rfind("https://", 0) == 0) {
            scheme = "https"; port = 443;
            auto rest = url.substr(8);
            auto slash = rest.find('/');
            host = (slash==std::string::npos) ? rest : rest.substr(0, slash);
            path = (slash==std::string::npos) ? "/" : rest.substr(slash);
        } else {
            return KDNET_ERR_HTTP_FAIL;
        }
        // optional: support host:port
        auto colon = host.find(':');
        if (colon != std::string::npos) {
            port = std::stoi(host.substr(colon+1));
            host = host.substr(0, colon);
        }
    }

    BOOL secure = (scheme=="https");
    HINTERNET hSession = WinHttpOpen(L"KDNet/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return KDNET_ERR_HTTP_FAIL;

    // Convert host/path to wide
    auto to_w = [](const std::string& s){
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w; w.resize(n? n-1:0);
        if(n>0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
        return w;
    };

    HINTERNET hConnect = WinHttpConnect(hSession, to_w(host).c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return KDNET_ERR_HTTP_FAIL; }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", to_w(path).c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return KDNET_ERR_HTTP_FAIL;
    }

    // Headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(
        hRequest,
        headers.c_str(), (DWORD)headers.size(),
        (LPVOID)json.data(), (DWORD)json.size(),
        (DWORD)json.size(), 0);

    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    DWORD status = 0; DWORD len = sizeof(status);
    if (ok) WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return (ok && status>=200 && status<300) ? KDNET_OK : KDNET_ERR_HTTP_FAIL;
#endif
}
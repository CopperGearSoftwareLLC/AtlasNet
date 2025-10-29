#include <cstdio>
#include <string>
#include <sstream>
#include "EntityJsonSender.hpp"  // Include helper functions

// ---- Export macros (cross-platform) ----
#if defined(_WIN32)
  #define KDNET_API extern "C" __declspec(dllexport)
  #define KDNET_CALL __cdecl
#else
  #define KDNET_API extern "C" __attribute__((visibility("default")))
  #define KDNET_CALL
#endif

static std::string g_endpoint = "http://127.0.0.1:7001/entity";

// -----------------------------------------------------------------------------
// Function: KDNetHelloWorld
// Purpose : Return a static "Hello World" string for Unity to print.
// Safety  : Returns a pointer to a static string (do NOT free it in C#).
// -----------------------------------------------------------------------------
KDNET_API const char* KDNET_CALL HelloWorld()
{
    return "Hello World from KDNet plugin!";
}

// -----------------------------------------------------------------------------
// Function: KDNetRegisterEntity
// Purpose : Test function that receives an entity ID + transform data
//           from Unity and prints/logs it.
// -----------------------------------------------------------------------------
KDNET_API int KDNET_CALL KDNetRegisterEntity(
    const char* entityId,
    float px, float py, float pz,
    float rx, float ry, float rz, float rw,
    float sx, float sy, float sz)
{
    // Build JSON
    std::ostringstream oss;
    oss << "{"
        << "\"id\":\"" << entityId << "\","
        << "\"position\":[" << px << "," << py << "," << pz << "],"
        << "\"rotation\":[" << rx << "," << ry << "," << rz << "," << rw << "],"
        << "\"scale\":[" << sx << "," << sy << "," << sz << "]"
        << "}";

    std::string json = oss.str();
    std::printf("[KDNet] Sending: %s\n", json.c_str());

    // Quick & dirty: fixed URL of your server mesher container
    return send_entity_to_server(json, g_endpoint);
}

/**
 * @brief Set the endpoint URL for sending entity data.
 * 
 * @param url 
 * @return KDNET_API 
 */
KDNET_API int KDNET_CALL KDNetSetEndpoint(const char* url) {
    if (url && url[0]) {
        g_endpoint = url;
    }
    return KDNET_OK;
}
#include <cstdio>
#include <string>
#include <sstream>
#include "KDNetUnityAPIHelper.hpp"  // Include helper functions

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
//           from Unity and prints/logs it. Later, this can push data to your
//           Docker server mesher via sockets, HTTP, or gRPC.
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
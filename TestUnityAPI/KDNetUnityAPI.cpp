#include <cstdio>

// ---- Export macros (cross-platform) ----
#if defined(_WIN32)
  #define KDNET_API extern "C" __declspec(dllexport)
  #define KDNET_CALL __cdecl
#else
  #define KDNET_API extern "C" __attribute__((visibility("default")))
  #define KDNET_CALL
#endif

// Return codes
enum KDNetStatus {
    KDNET_OK = 0,
    KDNET_ERR_NULL_ID = 1,
    KDNET_ERR_OTHER = 2
};

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
    std::printf("[KDNet] Entity Registered:\n");
    std::printf("  ID: %s\n", entityId);
    std::printf("  Position: (%.2f, %.2f, %.2f)\n", px, py, pz);
    std::printf("  Rotation (quat): (%.2f, %.2f, %.2f, %.2f)\n", rx, ry, rz, rw);
    std::printf("  Scale: (%.2f, %.2f, %.2f)\n", sx, sy, sz);

    return (entityId == nullptr) ? KDNET_ERR_NULL_ID : KDNET_OK;
}
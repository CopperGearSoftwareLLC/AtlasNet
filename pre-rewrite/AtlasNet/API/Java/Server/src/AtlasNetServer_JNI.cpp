#include <jni.h>
#include "Entity.hpp"
#include "AtlasNetServer.hpp"  // from ../src

// Example: Java class com.atlasnet.server.AtlasNetServer
// with native method: private static native void nativeInit();

extern "C" JNIEXPORT void JNICALL
Java_com_atlasnet_api_server_AtlasNetServer_nativeInit(JNIEnv* env, jclass clazz)
{
    AtlasNetServer::InitializeProperties props{};
    // fill props as needed...
    AtlasNetServer::Get().Initialize(props);
}
extern "C" JNIEXPORT void JNICALL
Java_com_atlasnet_api_server_AtlasNetServer_nativeTick(JNIEnv* env, jclass clazz)
{
    std::span<AtlasEntity> empty_span;
    std::vector<AtlasEntity> incoming;
    std::vector<AtlasEntity::EntityID> Outgoing;
    AtlasNetServer::Get().Update(empty_span,incoming,Outgoing);
}

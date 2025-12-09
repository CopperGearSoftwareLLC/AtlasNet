#include <jni.h>
#include "AtlasNetServer.hpp"  // from ../src

// Example: Java class com.atlasnet.server.AtlasNetServer
// with native method: private static native void nativeInit();

extern "C" JNIEXPORT void JNICALL
Java_com_atlasnet_server_AtlasNetServer_nativeInit(JNIEnv* env, jclass clazz)
{
    AtlasNetServer::InitializeProperties props{};
    // fill props as needed...
    AtlasNetServer::Get().Initialize(props);
}

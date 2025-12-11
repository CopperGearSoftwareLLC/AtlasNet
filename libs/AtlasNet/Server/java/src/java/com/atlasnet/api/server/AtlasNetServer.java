package com.atlasnet.api.server;

public final class AtlasNetServer {
    static {
        // Load your JNI library: libatlasnetserver_java.so / atlasnetserver_java.dll
        System.loadLibrary("atlasnetserver_java");
    }

    private AtlasNetServer() {}

    // JNI hook
    private static native void nativeInit();
    private static native void nativeTick();

    public static void init() {
        nativeInit();
    }
    public static void tick()
    {
        nativeTick();
    }
}

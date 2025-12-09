package com.atlasnet.server;

public final class AtlasNetServer {
    static {
        // Load your JNI library: libatlasnetserver_java.so / atlasnetserver_java.dll
        System.loadLibrary("atlasnetserver_java");
    }

    private AtlasNetServer() {}

    // JNI hook
    private static native void nativeInit();

    public static void init() {
        nativeInit();
    }
}

package com.atlasnet.fabric.server;

import net.fabricmc.api.DedicatedServerModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;

import com.atlasnet.api.server.*;
public final class ExampleModFabricServer implements DedicatedServerModInitializer {
    @Override
    public void onInitializeServer() {
        System.out.println("[AtlasNet] Fabric dedicated server init");
        AtlasNetServer.init();
        // Runs only on dedicated servers (not client, not integrated server).
        // Safe place to set up networking, config, background threads, etc.

        ServerTickEvents.END_SERVER_TICK.register(server -> {
            try {
                AtlasNetServer.tick();
            } catch (Throwable t) {
                // Don’t let an exception crash the server tick loop
                t.printStackTrace();
            }
        });
    }

    
}
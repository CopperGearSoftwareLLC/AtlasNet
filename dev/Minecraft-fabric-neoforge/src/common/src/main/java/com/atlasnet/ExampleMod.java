package com.atlasnet;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;



public final class ExampleMod {
    public static final String MOD_ID = "atlasnet";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);
    public static void init() {

        LOGGER.info("Initializing AtlasNetServer");
        // Write common init code here.
    }
}

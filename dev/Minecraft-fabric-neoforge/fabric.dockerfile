FROM itzg/minecraft-server:stable AS mc
#COPY src/fabric/build/libs/atlasnet-fabric-1.0.0.jar /mods

RUN apt-get update && apt-get install -y --no-install-recommends \
    libxtst6 libxi6 libxrender1 libxext6 libx11-6 libxcb1 libxau6 libxdmcp6 

COPY build/atlasnet-fabric-1.0.0.jar /mods/

# Copies the requires .so's
COPY natives/ /usr/local/lib/atlasnet/natives/
RUN set -eux; \
    mkdir -p /usr/lib; \
    find /usr/local/lib/atlasnet/natives -type f -name '*.so*' -exec cp -a {} /usr/lib/ \;

# optional but good practice
RUN ldconfig

ENV JVM_OPTS="-Djava.library.path=/usr/lib:/usr/local/lib:/lib:/usr/local/lib -Xcheck:jni -XX:ErrorFile=/data/hs_err_pid%p.log -XX:+CreateCoredumpOnCrash"

ENV EULA=TRUE \
    TYPE=FABRIC \
    VERSION=1.21.10\
    MAX_MEMORY=512M\
    MEMORY=512M\
    MODRINTH_PROJECTS="fabric-api,architectury-api" \
    MODRINTH_DOWNLOAD_DEPENDENCIES=required \
    INIT_MEMORY=12M\
    LEVEL_TYPE=minecraft:flat 


${GAME_SERVER_ENTRYPOINT}
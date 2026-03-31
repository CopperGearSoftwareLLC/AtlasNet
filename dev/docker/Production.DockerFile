FROM debian:bookworm-slim AS builder
WORKDIR /atlasnet
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libuv1-dev \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*
# Copy only src/include/cmake directories
COPY src/ src/
COPY include/ include/
COPY CMakeLists.txt CMakeLists.txt
COPY cmake/ cmake/

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DATLASNET_DEV=OFF -DATLASNET_EXAMPLES=OFF -DATLASNET_TESTS=OFF
RUN cmake --build build --parallel --target AtlasNetWatchdog AtlasNetProxy AtlasNetWebBackend

FROM debian:bookworm-slim AS runner
WORKDIR /atlasnet


FROM runner AS atlasnet-watchdog
COPY --from=builder /atlasnet/build/AtlasNetWatchdog /atlasnet/
ENTRYPOINT ["/atlasnet/AtlasNetWatchdog"]

FROM runner AS atlasnet-proxy
COPY --from=builder /atlasnet/build/AtlasNetProxy /atlasnet/
ENTRYPOINT ["/atlasnet/AtlasNetProxy"]


FROM runner AS atlasnet-web
#COPY --from=builder /atlasnet/build/AtlasNetWebBackend /atlasnet/
#COPY web/ /atlasnet/frontend/
#WORKDIR /atlasnet/frontend
#RUN npm install
#RUN npm run build
#ENTRYPOINT ["/atlasnet/AtlasNetWebBackend"]


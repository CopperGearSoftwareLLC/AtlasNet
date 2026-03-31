# Minimal atlasnetsdk stage
FROM ubuntu:24.04 AS runner
WORKDIR /atlasnet
RUN apt-get update && apt-get install -y libatomic1 binutils gdb build-essential
COPY ./.stage /atlasnet

FROM scratch AS atlasnetsdk
COPY ./.stage /atlasnet

# Watchdog stage
FROM runner AS watchdog
COPY --from=atlasnetsdk /atlasnet/watchdog /atlasnet/watchdog

# ARG to specify optional prelaunch command (default empty)
ARG PRELAUNCH_CMD=""
# Use JSON array syntax; if PRELAUNCH_CMD is empty, run default executable
# Dockerfile trick: split string into array only if set
ENTRYPOINT [ "sh", "-c", "if [ -z \"$PRELAUNCH_CMD\" ]; then exec /atlasnet/watchdog; else exec $PRELAUNCH_CMD /atlasnet/watchdog; fi" ]

# Proxy stage
FROM runner AS proxy
COPY --from=atlasnetsdk /atlasnet/proxy /atlasnet/proxy

ARG PRELAUNCH_CMD=""
ENTRYPOINT [ "sh", "-c", "if [ -z \"$PRELAUNCH_CMD\" ]; then exec /atlasnet/proxy; else exec $PRELAUNCH_CMD /atlasnet/proxy; fi" ]

# syntax=docker/dockerfile:1.10

FROM ubuntu:24.04
WORKDIR /RTS
RUN apt update && apt install -y libatomic1 binutils gdb build-essential
COPY .stage /RTS/
RUN ls -a && echo hi
RUN chmod +x /RTS/RTSServer
ENTRYPOINT ["/RTS/RTSServer"]
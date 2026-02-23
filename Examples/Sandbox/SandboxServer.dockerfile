# syntax=docker/dockerfile:1.10

FROM ubuntu:24.04
WORKDIR /sandbox
RUN apt update
RUN apt install -y libatomic1 binutils
COPY .stage /sandbox/
RUN ls -a && echo hi
RUN chmod +x /sandbox/SandboxServer
ENTRYPOINT ["/sandbox/SandboxServer"]
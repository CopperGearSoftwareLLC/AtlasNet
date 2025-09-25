
# Use an official Ubuntu base image
FROM ubuntu:25.04
# Set working directory
WORKDIR /app

# Copy local files into the image
# Install dependencies
RUN apt-get update && apt-get install -y gdbserver docker.io libprotobuf-dev protobuf-compiler libssl-dev libcurl4-openssl-dev curl
 
COPY ../bin/DebugDocker/God/God /app/
COPY KDNetVars.sh /app/
COPY Start.sh /app/
# For GDBserver
EXPOSE 1234 
# Set default command
CMD /bin/bash -c "gdbserver :1234 god "


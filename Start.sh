#!/bin/bash
#!/bin/bash
. ./KDNetVars.sh   
# Run docker-compose


# Path to Partition



# Check if at least one argument is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 God|Partition"
    exit 1
fi


ARG="$1"



case "$ARG" in
    God)
        # Launch watcher in background to clean up partitions when God stops
        # nohup bash ./god_watcher.sh & disown

        echo "Runing God"
        docker rm -f $GOD_CONTAINER_NAME 2>/dev/null || true 
        docker run --init -v /var/run/docker.sock:/var/run/docker.sock --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name $GOD_CONTAINER_NAME -d -p 1234:1234 $GOD_IMAGE_NAME
        #docker run --init --stop-timeout=4 --stop-signal=SIGTERM -v /var/run/docker.sock:/var/run/docker.sock --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name $GOD_CONTAINER_NAME -d -p 1234:1234 $GOD_IMAGE_NAME

        ;;

    Partition)
        if [ $# -lt 3 ]; then
            echo "Usage: $0 Partition <id> <port>"
            exit 1
        fi
        ID=$2
        PORT=$3
        echo "Spawning Partition $ID on port $PORT"

        # Call Docker REST API via curl (inside WSL with mounted socket)
        curl --unix-socket /var/run/docker.sock -X POST \
            -H "Content-Type: application/json" \
            -d "{
                  \"Image\": \"$PARTITION_IMAGE_NAME\",
                  \"Env\": [
                      \"PARTITION_ID=$ID\"
                  ],
                  \"ExposedPorts\": {
                      \"1234/tcp\": {},
                    \"1235/tcp\": {},
                    \"1236/tcp\": {}
                  },
                  \"HostConfig\": {
                      \"PortBindings\": {
                        \"1234/tcp\": [{\"HostPort\": \"$PORT\"}],
                        \"1235/tcp\": [{\"HostPort\": \"1235\"}],
                        \"1236/tcp\": [{\"HostPort\": \"1236\"}]
                      }
                  }
                }" \
            http://localhost/containers/create?name=partition_$ID

        # Start container
        curl --unix-socket /var/run/docker.sock -X POST \
            http://localhost/containers/partition_$ID/start

        echo "✅ Partition $ID started on port $PORT"
        ;;


    *)
        echo "Unknown argument: $ARG"
        echo "Usage: $0 God|Partition"
        exit 1
        ;;
esac


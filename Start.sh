#!/bin/bash
#!/bin/bash
. ./AtlasNetVars.sh   
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
        nohup bash ./god_watcher.sh & disown

        echo "Runing God"
        docker rm -f $GOD_CONTAINER_NAME 2>/dev/null || true 
        docker network create AtlasNet
        docker run --network AtlasNet --init -v /var/run/docker.sock:/var/run/docker.sock --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name $GOD_CONTAINER_NAME -d -p 1234:1234 $GOD_IMAGE_NAME
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

        docker network inspect AtlasNet >/dev/null 2>&1 || docker network create AtlasNet

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

    Redis)
        echo "Starting Redis database container"
        docker rm -f $REDIS_CONTAINER_NAME 2>/dev/null || true
        docker run -d \
          --name $REDIS_CONTAINER_NAME \
          -p $REDIS_PORT:6379 \
          $REDIS_IMAGE_NAME \
          --save 60 1 --loglevel warning
        echo "✅ Redis started on localhost:$REDIS_PORT"
        ;;

        Postgres)
            echo "Starting Postgres database container"
            docker rm -f $POSTGRES_CONTAINER_NAME 2>/dev/null || true
            docker volume create postgres_data >/dev/null 2>&1
            docker run -d \
              --name $POSTGRES_CONTAINER_NAME \
              -e POSTGRES_USER=$POSTGRES_USER \
              -e POSTGRES_PASSWORD=$POSTGRES_PASSWORD \
              -e POSTGRES_DB=$POSTGRES_DB \
              -p $POSTGRES_PORT:5432 \
              -v postgres_data:/var/lib/postgresql/data \
              $POSTGRES_IMAGE_NAME
            echo "✅ Postgres started on localhost:$POSTGRES_PORT"
            echo "   User: $POSTGRES_USER  Pass: $POSTGRES_PASSWORD  DB: $POSTGRES_DB"
            ;;



    *)
        echo "Unknown argument: $ARG"
        echo "Usage: $0 God|Partition"
        exit 1
        ;;
esac


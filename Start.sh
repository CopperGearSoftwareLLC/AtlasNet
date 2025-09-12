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
        echo "Runing God"
        docker run -v /var/run/docker.sock:/var/run/docker.sock --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name God-Container -d -p :1234 $GOD_IMAGE_NAME
        ;;

    Partition)
        echo "Running Partition Compose"
        docker compose -p gameserver -f $DOCKER_FILES_PATH/$PARTITION_GAMESERVER_COMPOSE_FILE up --remove-orphans 
        ;;

    *)
        echo "Unknown argument: $ARG"
        echo "Usage: $0 God|Partition|GameServer <path>"
        exit 1
        ;;
esac


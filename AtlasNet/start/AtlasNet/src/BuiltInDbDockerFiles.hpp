#include "Misc/String_utils.hpp"
#include "MiscDockerFiles.hpp"

DOCKER_FILE_DEF BuiltInDbStack =
	MacroParse(R"(
  ${BUILTINDB_REDIS_SERVICE_NAME}:
    image: valkey/valkey:latest
    command: ["valkey-server", "--appendonly", "yes", "--port", "${BUILTINDB_REDIS_PORT}"]
    networks: [${ATLASNET_NETWORK_NAME}]
    ports:
      - target: ${BUILTINDB_REDIS_PORT}
        published: ${BUILTINDB_REDIS_PORT}
        protocol: tcp
        mode: ingress
      - target: ${BUILTINDB_REDIS_PORT}
        published: ${BUILTINDB_REDIS_PORT}
        protocol: udp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
  ${BUILTINDB_POSTGRES_SERVICE_NAME}:
    image: postgres:16-alpine
    command: ["postgres", "-c", "port=${BUILTINDB_POSTGRES_PORT}"]
    environment:
      POSTGRES_PASSWORD: postgres
    volumes:
      - pgdata:/var/lib/postgresql/data
    networks: [${ATLASNET_NETWORK_NAME}]
volumes:
  pgdata:
    driver: local
)",
			   {{"BUILTINDB_REDIS_SERVICE_NAME", _BUILTINDB_REDIS_SERVICE_NAME},
				{"BUILTINDB_REDIS_PORT", std::to_string(_BUILTINDB_REDIS_PORT)},
				{"BUILTINDB_POSTGRES_SERVICE_NAME", _BUILTINDB_POSTGRES_SERVICE_NAME},
				{"BUILTINDB_POSTGRES_PORT", std::to_string(_BUILTINDB_POSTGRES_PORT)},
				{"ATLASNET_NETWORK_NAME", _ATLASNET_NETWORK_NAME}});
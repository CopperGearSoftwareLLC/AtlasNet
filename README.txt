# 🗺️ AtlasNet

Distributed Spatial Networking Framework

---
## Table of Contents

- [About](#about)
- [Features](#features)
- [Usage](#usage)
- [Configuration](#configuration)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

---
## About

Provide a detailed description of the project here.  
Explain why it exists, what problems it solves, and who it’s for.

---
## Features
---
## Usage
AtlasNet has enviroment variables that control how it functions

| Variable | Description | Default | Example |
|----------|-------------|---------|---------|
| 

---
## 🏗 Architecture Overview
```mermaid
flowchart LR

    %% External
    Client[Client]

    %% AtlasNet System Boundary
    subgraph AtlasNet
        Watchdog[Watchdog]
        Proxy[Proxy]
        Shard[Shard]
        InternalDB[(InternalDB
        Valkey - transient)]
        TaskWorker[TaskWorker
        Compute Farm]
    end

    %% Network Flow
    Client -->|UDP| Proxy

    %% Internal Routing
    Proxy --> Shard
    Proxy --> TaskWorker

    %% Shard Dependencies
    Shard --> InternalDB
    Shard --> TaskWorker

    %% Supervision
    Watchdog  .-> Proxy
    Watchdog .-> Shard
    Watchdog .-> TaskWorker
    Watchdog .-> InternalDB
```


## Requirements

¯\\_(ツ)_/¯


## Build
¯\\_(ツ)_/¯

---
## Development

### Server

A game server must implement our AtlasNetServer interface

### Client

A game client must implement our AtlasNetClient interface

---

## Running

### <img src="docs/assets/docker-mark-blue.svg" width="24" height="24"/> Docker Swarm

Docker Stack example
```yml
version: "3.8"

services:
  WatchDog:
    image: watchdog
    networks: [AtlasNet]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock # REQUIRED
    deploy:
      labels:
        atlasnet.role: watchdog # REQUIRED
      placement:
          constraints:
            - 'node.role == manager'
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure

  Shard:
    image: shard:latest
    networks: [AtlasNet]
    deploy:
     labels:
        atlasnet.role: shard # REQUIRED
     resources:
        limits:
          cpus: "1.0"      # 1 core
          memory: 1G       # 1 GiB
     mode: replicated
     replicas: 0   # MUST BE 0
     restart_policy:
       condition: on-failure

  InternalDB:
    image: valkey/valkey:latest
    command: ["valkey-server", "--appendonly", "yes", "--port", "6379"]
    networks: [AtlasNet]
    ports:
      - target: 6379
        published: 6379
        protocol: tcp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
  Cartograph:
    image: cartograph
    networks: [AtlasNet]
    ports:
      - "3000:3000"   # Next.js default
      - "9229:9229"   # Node inspector
    deploy:
      placement:
          constraints:
            - 'node.role == manager'
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
  Proxy:
    image: ${REGISTRY_ADDR_OPT}proxy:latest
    networks: [AtlasNet]
    ports:
      - target: 25568
        published: 2555
        protocol: tcp
        mode: ingress
      - target: 25568
        published: 2555
        protocol: udp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
#  BuiltInDB_Redis:
#    image: valkey/valkey:latest
#    command: ["valkey-server", "--appendonly", "yes", "--port", "2380"]
#    networks: [AtlasNet]
#    ports:
#      - target: 2380
#        published: 2380
#        protocol: tcp
#        mode: ingress
#      - target: 2380
#        published: 2380
#        protocol: udp
#        mode: ingress
#    deploy:
#      mode: replicated
#      replicas: 1
#      restart_policy:
#        condition: on-failure
#  BuiltInDB_PostGres:
#    image: postgres:16-alpine
#    command: ["postgres", "-c", "port=5432"]
#    environment:
#      POSTGRES_PASSWORD: postgres
#    volumes:
#      - pgdata:/var/lib/postgresql/data
#    networks: [AtlasNet]
volumes:
  pgdata:
    driver: local

networks:
  AtlasNet:
    name: AtlasNet
```
### <img src="docs/assets/Kubernetes_logo.svg" width="24" height="24"/> Kubernetes
---

## Dev container

The repo includes a **dev container** (`.devcontainer/`) with:

- vcpkg, Docker-in-Docker, Node.js (LTS)
- Extensions: CMake, Clangd, Docker, etc.

Use it in VS Code/Cursor with “Reopen in Container” to get a consistent build and run environment.
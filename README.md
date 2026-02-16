# AtlasNet

A **distributed game backend** in C++ for sharded game worlds. **Shards** (partitions) claim spatial regions (bounds) and run game logic; **WatchDog** manages partition lifecycle and heuristics; **Interlink** provides UDP networking between services; **Cartograph** is a web dashboard for network telemetry and topology. State is backed by **InternalDB** (Valkey/Redis) and optionally **BuiltInDB** (Postgres + Redis).

**Repository:** [github.com/CamilaBasualdoCibils/AtlasNet](https://github.com/CamilaBasualdoCibils/AtlasNet) — use the **`Development`** branch for the latest code.

```bash
git clone --branch Development https://github.com/CamilaBasualdoCibils/AtlasNet.git
```

## Overview

- **WatchDog** — Runs the partition heuristic (e.g. grid), health checks, and coordinates with InternalDB. Shards claim bounds from HeuristicManifest; WatchDog drives the layout.
- **Shards (Partitions)** — Each shard claims a region (bounds) via HeuristicManifest, runs Interlink for connectivity, and reports health and connection telemetry. **Entity handoff** (transfer of entity authority when entities cross bounds) is WIP.
- **Proxy** — Service that runs Interlink and participates in the network; client-facing routing and coordinator replacement are being implemented separately (see below).
- **Interlink** — UDP networking layer (Steam GameNetworkingSockets) used by WatchDog, Shard, and Proxy for peer-to-peer connections, health pings, and connection telemetry (NetworkManifest → InternalDB).
- **Cartograph** — Next.js dashboard that visualizes connection telemetry, shard topology, and heuristic state from InternalDB.

**Deprecated / in progress:**

- **Game Coordinator** — Deprecated. A new solution is being implemented (separate from this repo’s current runtime).
- Code under **`exc/`** directories is **deprecated** and should not be used or referenced; only non-`exc` sources are current.

## Architecture (high level)

```
    [ WatchDog ]  ← heuristic, health checks, shard layout
          |
    [ InternalDB ] (Valkey)  ← manifests, health, telemetry
          |
    [ Shard 1 ]   [ Shard 2 ]   [ Shard 3 ]   [ Proxy ]   ...
          |             |             |            |
          └─────────────┴─────────────┴────────────┘
                    Interlink (UDP)
          |
    [ Cartograph ]  ← web UI (telemetry, topology)
```

## Repository layout

Matches the [Development tree](https://github.com/CamilaBasualdoCibils/AtlasNet/tree/Development):

| Path | Description |
|------|--------------|
| `AtlasNet/` | Core AtlasNet library and runtimes |
| `AtlasNet/lib/` | Libraries: Interlink, Database, Entity, Heuristic, InternalDB, Web, etc. (use only non-`exc` code) |
| `AtlasNet/runtime/` | Services: **watchdog**, **shard**, **proxy** (from `src/` only), **cartograph** (web) |
| `AtlasNet/API/` | Client and Server APIs (C++, Unity, Java optional) |
| `AtlasNet/start/AtlasNet/` | Bootstrap / Docker stack and orchestration |
| `Examples/` | Sandbox client + server and other samples |
| `.devcontainer/` | Dev container (vcpkg, Docker-in-Docker, Node) |
| `.vscode/` | Editor config and tasks |
| `CMakeLists.txt` | Root CMake; adds AtlasNet and Examples |

Root also includes `.clang-format`, `.clang-tidy`, `.clangd`, and `Doxyfile` for formatting and docs. Do **not** rely on code in any **`exc/`** folder; it is deprecated.

## Requirements

- **CMake** 3.6+
- **C++20** compiler
- **vcpkg** (recommended) or CMake FetchContent for dependencies

Dependencies (via vcpkg): GameNetworkingSockets, Boost (multi_index, uuid, static_string, etc.), nlohmann_json, Redis/hiredis, libuv, glm, CURL, ZLIB.

Optional for Cartograph: **Node.js** (LTS) for the Next.js dashboard.

## Build

1. **Install vcpkg** and set `VCPKG_ROOT` (or use the dev container, which includes vcpkg).

2. **Configure and build** from repo root (clone the [Development](https://github.com/CamilaBasualdoCibils/AtlasNet/tree/Development) branch for latest):

   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build build
   ```

   Or use the **dev container** (see below), which configures vcpkg and the environment.

3. **CMake options** (in `AtlasNet/CMakeLists.txt`):

   - `ATLASNET_VCPKG_MODE` — Use vcpkg for deps (default ON).
   - `ATLASNET_INCLUDE_WEB` — Build Cartograph and Web lib (default OFF).
   - `ATLASNET_INCLUDE_RUNTIME` — Build watchdog, shard, proxy (default ON).
   - `ATLASNET_INCLUDE_API` — Build Client/Server API (default ON).
   - `ATLASNET_INCLUDE_BOOTSTRAP` — Build AtlasNet bootstrap/orchestration (default ON).

   Example with Cartograph:

   ```bash
   cmake -B build -S . \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
     -DATLASNET_INCLUDE_WEB=ON
   cmake --build build
   ```

## Run (local / dev)

- **InternalDB** (Valkey/Redis) must be reachable on the port defined in `Definitions.cmake` (e.g. 6379 or overridden for BuiltInDB).
- Run **WatchDog** first; it drives the heuristic and health checks.
- Run **Shard** and **Proxy** processes as needed; they use Interlink to connect and report telemetry.
- **Cartograph**: from `AtlasNet/runtime/cartograph/web`, run `npm install` and `npm run dev` (or `npm run dev:all` if you use the native server). Open http://localhost:3000 to view the dashboard.

For full Docker/Swarm-based deployment, use the **AtlasNet Bootstrap** (start/AtlasNet) and the Docker Compose / stack files referenced there (see `AtlasNet/start/AtlasNet/` and related headers).

## Dev container

The repo includes a **dev container** (`.devcontainer/`) with:

- vcpkg, Docker-in-Docker, Node.js (LTS)
- Extensions: CMake, Clangd, Docker, etc.

Use it in VS Code/Cursor with “Reopen in Container” to get a consistent build and run environment.

## Key components (for contributors)

- **Interlink** (`AtlasNet/lib/Interlink/src/`) — Connection lifecycle, packets, ServerRegistry, ProxyRegistry, health pings, connection telemetry (NetworkManifest). Use only sources under `src/`; ignore `exc/`.
- **Shard** (`AtlasNet/runtime/shard/src/`) — Partition, EntityAtBoundsManager, EntityHandoff (in progress). Entity authority handoff across bounds is not yet pushed.
- **Telemetry** — NetworkManifest writes connection telemetry to InternalDB; Cartograph (and optional Web/NetworkTelemetry API) read and visualize it.
- **Heuristic** (`AtlasNet/lib/Heuristic/src/`) — Partition layout (e.g. GridHeuristic); WatchDog and HeuristicManifest drive which bounds exist and who claims them.

## Ports (defaults, from `Definitions.cmake`)

| Service       | Port  |
|---------------|-------|
| WatchDog      | 25564 |
| Shard         | 25565 |
| Game server   | 25566 |
| Proxy         | 25568 |
| Game client   | 25569 (temp) |
| InternalDB    | 6379  |
| BuiltInDB Redis | 2380 |
| BuiltInDB Postgres | 5432 |
| Cartograph (Next.js) | 3000 |
| Registry      | 5000  |

## Goals

- Keep authority transfer behavior easy to reason about.
- Use a transfer tick (`future time`) so sender and receiver can switch in sync.
- many edge cases were not considered, in favor of being good enough for immidate single entity testing

## Components

- `NH_EntityAuthorityManager`
  - Runtime orchestrator.
  - Runs ownership election, simulation tick, boundary checks, send/adopt, and commit.
- `NH_EntityAuthorityTracker`
  - Holds local authority state and exports telemetry rows.
- `NH_HandoffPacketManager`
  - Sends/receives handoff packets and dispatches callbacks.
- `NH_HandoffConnectionManager`
  - Manages peer connection activity and cleanup.
- `NH_HandoffConnectionLeaseCoordinator`
  - Optional Redis lease coordination to reduce duplicate connection contention.

## Runtime Flow

1. Owner shard simulates entities and updates tracker snapshots.
2. If an entity leaves local claimed bounds and enters another shard's claimed
   bound, sender emits a handoff packet with `transferTick = now + lead`.
3. Receiver stores incoming entity as pending.
4. At/after `transferTick`, receiver adopts entity state.
5. Sender commits outgoing transfer at/after `transferTick` and drops local authority.

## Telemetry

- Telemetry persistence is handled through `AuthorityManifest`.
- `AuthorityManifest` accepts its own row DTO and does not depend directly on
  `NH_EntityAuthorityTracker`.

## Current Simulation

- Debug orbit simulator is used to generate deterministic moving entities.
- Test entity count is controlled by compile-time macro in
  `NH_EntityAuthorityManager.cpp`:
  - `ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT`
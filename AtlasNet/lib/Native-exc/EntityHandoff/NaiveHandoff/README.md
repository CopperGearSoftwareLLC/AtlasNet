## NaiveHandoff (First Version)

This is the first handoff version.
It was made to get basic handoff working quickly.

It is simple on purpose and does not handle every edge case.

## Simple Protocol

- sender picks a future tick: `transferTick = now + lead`
- sender sends full entity snapshot to target shard
- target stores it as pending
- when local tick reaches `transferTick`, target adopts the entity
- when sender reaches `transferTick`, sender drops local authority

## Known Weak Spots

- can only handle 1 entity

## Runtime Flow (Per Tick)

1. Update owner selection.
2. Simulate local entities.
3. Check if entities crossed a bound.
4. Send handoff packets for crossing entities.
5. Adopt incoming handoffs when due.
6. Commit outgoing handoffs when due.
7. Publish telemetry.

## File Responsibilities

- `NH_EntityAuthorityManager.hpp/.cpp`
  - Main runtime loop for NaiveHandoff.
  - Wires simulation, handoff, and commit together.

- `NH_EntityAuthorityTracker.hpp/.cpp`
  - Stores authority state for local entities.
  - Tracks `authoritative` vs `passing`.

- `NH_HandoffPacketManager.hpp/.cpp`
  - Sends and receives handoff packets.
  - Calls runtime callbacks for incoming handoffs.

- `NH_HandoffConnectionManager.hpp/.cpp`
  - Tracks active peer links.
  - Cleans up inactive links.

- `NH_HandoffConnectionLeaseCoordinator.hpp/.cpp`
  - Optional Redis lease helper to reduce duplicate connections.

## Telemetry

- Telemetry is written through `AuthorityManifest`.
- Naive runtime uses tracker rows and converts them for persistence.

## Main Test Knob

- `ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT`
  - Compile-time entity count for debug orbit simulation.

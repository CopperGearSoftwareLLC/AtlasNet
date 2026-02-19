## implementation considerations:
sometimes entities seem to be stuck in limbo of no entity claiming. we need to resolve this and find a server for them. perhaps consider watchdog?

## Goals

- Keep `ServerHandoff` as a separate second iteration from `NaiveHandoff`.
- Keep files small and responsibilities isolated.
- Avoid one central runtime class owning all cross-cutting logic.

## Components

- `SH_ServerAuthorityManager`
  - Public facade used by `AtlasNetServer`.
- `SH_ServerAuthorityRuntime`
  - Thin orchestrator for lifecycle and tick ordering.
- `SH_OwnershipElection`
  - Owns owner election policy/state and owner transitions.
- `SH_BorderHandoffPlanner`
  - Detects boundary exits and emits/sends outgoing handoff intents for all
    crossing entities in a tick.
- `SH_TransferMailbox`
  - Owns per-entity pending incoming/outgoing transfer-tick state and
    commit/adopt behavior.
- `SH_TelemetryPublisher`
  - Isolated tracker-to-manifest bridge.

## Dependency Direction

- Runtime depends on components.
- Components do not depend on runtime.
- Shared handoff DTOs are isolated in `SH_HandoffTypes.hpp`.
- Pending transfer state is keyed by `entityId` to avoid single-slot overwrite
  bugs when many entities hand off concurrently.

## Notes

- This iteration currently reuses Naive transport/tracker primitives:
  - `NH_HandoffPacketManager`
  - `NH_HandoffConnectionManager`
  - `NH_EntityAuthorityTracker`
- Next step is to replace those with ServerHandoff-native versions once protocol
  and lifecycle settle.

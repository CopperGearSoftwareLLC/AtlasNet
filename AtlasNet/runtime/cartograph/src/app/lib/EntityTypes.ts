export interface AtlasEntity {
  entityId: string;
  ownerId: string;
  world: number;
  position: {
    x: number;
    y: number;
    z: number;
  };
  isClient: boolean;
  clientId: string;
}

export function parseEntityView(
  raw: Record<string, any[]>
): Map<number, AtlasEntity[]> {
  const result = new Map<number, AtlasEntity[]>();
  for (const [boundIdStr, entries] of Object.entries(raw)) {
    const boundId = Number(boundIdStr);
    console.log(
      "ParseEntityView parse ID",
      boundId,
      "-",
      entries?.length ?? 0,
      "entries"
    );

    if (!Array.isArray(entries)) continue;

    const parsed: AtlasEntity[] = entries.map((entry) => ({
      entityId: entry.EntityID,
      ownerId: entry.ClientID,
      world: entry.WorldID,
      position: {
        x: entry.position?.x ?? 0,
        y: entry.position?.y ?? 0,
        z: entry.position?.z ?? 0,
      },
      isClient: entry.ISClient,
      clientId: entry.ClientID,
    }));
    if (parsed.length > 0) {
      const firstPos = parsed[0].position;
      console.log(
        `Bound ${boundId} first entity position -> x:${firstPos.x} y:${firstPos.y} z:${firstPos.z}`
      );
    } else {
      console.log(`Bound ${boundId} has no entities`);
    }
    result.set(boundId, parsed);
  }
  return result;
}
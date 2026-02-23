function readField(entry, keyCandidates) {
  if (!entry || typeof entry !== 'object') {
    return undefined;
  }

  for (const key of keyCandidates) {
    let raw;
    try {
      raw = entry[key];
    } catch {
      continue;
    }

    if (typeof raw === 'undefined') {
      continue;
    }

    if (typeof raw === 'function') {
      try {
        return raw.call(entry);
      } catch {
        continue;
      }
    }
    return raw;
  }

  return undefined;
}

function toNumber(value, fallback = 0) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function toBoolean(value) {
  if (value === true || value === 1) {
    return true;
  }
  const normalized = String(value ?? '').trim().toLowerCase();
  return (
    normalized === '1' ||
    normalized === 'true' ||
    normalized === 'yes' ||
    normalized === 'on'
  );
}

function readOwnerByBoundIdFromHeuristic(addon) {
  const ownerByBoundId = new Map();
  if (!addon || !addon.HeuristicDraw || !addon.std_vector_IBoundsDrawShape_) {
    return ownerByBoundId;
  }

  try {
    const heuristicDraw = new addon.HeuristicDraw();
    const shapes = new addon.std_vector_IBoundsDrawShape_();
    heuristicDraw.DrawCurrentHeuristic(shapes);

    for (let i = 0; i < shapes.size(); i += 1) {
      const shape = shapes.get(i);
      const boundId = String(shape?.id ?? '').trim();
      const ownerId = String(shape?.owner_id ?? '').trim();
      if (!boundId || !ownerId) {
        continue;
      }
      ownerByBoundId.set(boundId, ownerId);
    }
  } catch {
    return ownerByBoundId;
  }

  return ownerByBoundId;
}

function readEntityLedgersTelemetry(addon, entityLedgersView, options = {}) {
  if (!addon || !entityLedgersView || !addon.std_vector_EntityLedgerEntry_) {
    return [];
  }

  const ownerByBoundIdFromDb =
    options.ownerByBoundId && typeof options.ownerByBoundId === 'object'
      ? options.ownerByBoundId
      : {};
  const ownerByBoundIdFallback = readOwnerByBoundIdFromHeuristic(addon);
  const entities = new addon.std_vector_EntityLedgerEntry_();
  entityLedgersView.GetEntityLists(entities);

  const rows = [];
  for (let i = 0; i < entities.size(); i += 1) {
    const entry = entities.get(i);
    const entityId = String(
      readField(entry, ['EntityID', 'entityId', 'EntityId']) ?? ''
    ).trim();
    if (!entityId) {
      continue;
    }

    const boundId = String(
      readField(entry, ['BoundID', 'boundId', 'BoundId']) ?? ''
    ).trim();
    const ownerId =
      (boundId ? ownerByBoundIdFromDb[boundId] : '') ||
      ownerByBoundIdFallback.get(boundId) ||
      (boundId ? `bound:${boundId}` : 'unknown');

    const world = toNumber(readField(entry, ['WorldID', 'worldId', 'WorldId']), 0);
    const x = toNumber(readField(entry, ['positionx', 'x', 'PositionX']), 0);
    const y = toNumber(readField(entry, ['positiony', 'y', 'PositionY']), 0);
    const z = toNumber(readField(entry, ['positionz', 'z', 'PositionZ']), 0);
    const isClient = toBoolean(
      readField(entry, ['ISClient', 'isClient', 'IsClient'])
    );
    const clientId = String(
      readField(entry, ['ClientID', 'clientId', 'ClientId']) ?? ''
    ).trim();

    rows.push([
      entityId,
      ownerId,
      String(world),
      String(x),
      String(y),
      String(z),
      isClient ? '1' : '0',
      clientId,
    ]);
  }

  return rows;
}

module.exports = {
  readEntityLedgersTelemetry,
};

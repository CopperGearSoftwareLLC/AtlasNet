
// native-server.js
const express = require('express');
const cors = require('cors');
const addon = require('../nextjs/native/Web.node');
const { HeuristicDraw, IBoundsDrawShape, std_vector_IBoundsDrawShape_ } = addon; // your .node file
global.nodeJsWrapper = addon.NodeJSWrapper
  ? new addon.NodeJSWrapper()
  : null;
function decodeConnectionRow(row) {
  const hasShardId = row.length >= 14;
  const offset = hasShardId ? 1 : 0;

  return {
    shardId: hasShardId ? row[0] : null,
    IdentityId: row[offset],
    targetId: row[offset + 1],
    pingMs: Number(row[offset + 2]),
    inBytesPerSec: Number(row[offset + 3]),
    outBytesPerSec: Number(row[offset + 4]),
    inPacketsPerSec: Number(row[offset + 5]),
    pendingReliableBytes: Number(row[offset + 6]),
    pendingUnreliableBytes: Number(row[offset + 7]),
    sentUnackedReliableBytes: Number(row[offset + 8]),
    queueTimeUsec: Number(row[offset + 9]),
    qualityLocal: Number(row[offset + 10]),
    qualityRemote: Number(row[offset + 11]),
    state: row[offset + 12],
  };
}
function computeShardAverages(connections) {
  if (!connections || connections.length === 0) {
    return { inAvg: 0, outAvg: 0 };
  }

  let inSum = 0;
  let outSum = 0;

  for (const c of connections) {
    inSum += c.inBytesPerSec;
    outSum += c.outBytesPerSec;
  }

  return {
    inAvg: inSum / connections.length,
    outAvg: outSum / connections.length,
  };
}

function inspectObject(name, obj) {
  console.log(`${name} keys:`, Object.keys(obj));
  if (obj.prototype) {
    console.log(`${name} prototype functions:`, Object.getOwnPropertyNames(obj.prototype));
  }
}
for (const [key, value] of Object.entries(addon)) {
  console.log(`\n--- Inspecting ${key} ---`);
  inspectObject(key, value);
}

const app = express();
app.use(cors()); // allow your frontend to call it

const nt = new addon.NetworkTelemetry();

app.get('/networktelemetry', (req, res) => {
  try {
    const { NetworkTelemetry, std_vector_std_string_, std_vector_std_vector_std_string__ } = addon;

    // SWIG string vector
    const idsVec = new std_vector_std_string_();
    const healthVec = new std_vector_std_string_();
    const telemetryVec = new std_vector_std_vector_std_string__();

    nt.GetLivePingIDs(idsVec, healthVec);
    nt.GetAllTelemetry(telemetryVec);
    // Convert idsVec and healthVec to JS arrays
    const ids = [];
    const healthByShard = new Map();

    const count = Math.min(idsVec.size(), healthVec.size());
    for (let i = 0; i < count; i++) {
      const shardId = String(idsVec.get(i));
      const health = Number(healthVec.get(i)); // ping ms

      ids.push(shardId);
      healthByShard.set(shardId, health);
    }


    // Convert telemetryVec: std::vector<std::vector<std::string>> -> string[][]
    const allRows = [];
    for (let i = 0; i < telemetryVec.size(); i++) {
      const rowVec = telemetryVec.get(i);
      const row = [];
      for (let j = 0; j < rowVec.size(); j++) {
        row.push(String(rowVec.get(j)));
      }
      allRows.push(row);
    }

    /**
     * Heuristic grouping:
     * - If rows include a shardId as first column, group by row[0].
     * - Otherwise: attach the whole dump to each shard (still “displays data”).
     */
    const rowsByShard = new Map();
    for (const row of allRows) {
      if (row.length < 13) {
        continue;
      }

      const decoded = decodeConnectionRow(row);
      const shardId = decoded.shardId ?? decoded.IdentityId;

      if (!rowsByShard.has(shardId)) {
        rowsByShard.set(shardId, []);
      }
      rowsByShard.get(shardId).push(decoded);
      //console.log(`Decoded row for shard ${shardId}:`, decoded);
    }

    // Upload/Download: average from connections
    const telemetry = ids.map((id) => {
      const connections = rowsByShard.get(id) ?? [];
      const { inAvg, outAvg } = computeShardAverages(connections);

      return {
        shardId: id,
        downloadKbps: inAvg,   // avg inBytesPerSec
        uploadKbps: outAvg,   // avg outBytesPerSec
        connections,
      };
    });



    res.json(telemetry);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Native addon failed' });
  }
});

const entityLedgersView = addon.EntityLedgersView
  ? new addon.EntityLedgersView()
  : null;
const authorityTelemetry = addon.AuthorityTelemetry
  ? new addon.AuthorityTelemetry()
  : null;
app.get('/authoritytelemetry', (req, res) => {
  try {
    if (!entityLedgersView || !addon.std_vector_EntityLedgerEntry_) {
      res.json([]);
      return;
    }
    console.log(`EntityLedgersView Init`);

    const EntityList = new addon.std_vector_EntityLedgerEntry_();

    // Call native function
    console.log(`EntityLedgersView Fetch`);

    entityLedgersView.GetEntityLists(EntityList);

    console.log(`EntityLedgersView returned ${EntityList.size()} entries `);

    const result = {}; // This will hold BoundID -> list of entities

    for (let j = 0; j < EntityList.size(); j++) {
      const e = EntityList.get(j);

      const entityData = {
        EntityID: e.EntityID,
        ClientID: e.ClientID,
        ISClient: e.ISClient,
        BoundID: e.BoundID,
        WorldID: e.WorldID,
        position: { x: e.positionx, y: e.positiony, z: e.positionz }
      };

      // Use BoundID as the key
      const boundID = Number(e.BoundID);

      if (!result[boundID]) {
        result[boundID] = []; // initialize array if doesn't exist
      }
      result[boundID].push(entityData); // append entity to its BoundID list
    }
    res.json(result);

  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Authority telemetry fetch failed' });
  }
});
/*  try {
    if (!authorityTelemetry || !addon.std_vector_std_vector_std_string__) {
      res.json([]);
      return;
    }
 
    const telemetryVec = new addon.std_vector_std_vector_std_string__();
    authorityTelemetry.GetAllTelemetry(telemetryVec);
 
    const rows = [];
    for (let i = 0; i < telemetryVec.size(); i++) {
      const rowVec = telemetryVec.get(i);
      const row = [];
      for (let j = 0; j < rowVec.size(); j++) {
        row.push(String(rowVec.get(j)));
      }
      rows.push(row);
    }
 
    // Expected row schema:
    // [entityId, ownerId, world, x, y, z, isClient, clientId]
    res.json(rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Authority telemetry fetch failed' });
  } */
app.get('/heuristic', (req, res) => {
  try {
    const hd = new HeuristicDraw();

    // ✅ Create SWIG-compatible vector
    const shapesVector = new std_vector_IBoundsDrawShape_();

    // Call native function
    hd.DrawCurrentHeuristic(shapesVector);

    // Convert to plain JS
    const shapes = [];
    for (let i = 0; i < shapesVector.size(); i++) {
      const shape = shapesVector.get(i);

      // Convert vertices from std_vector_std_pair_float_float__ to array of vec2
      const vertices = [];
      for (let j = 0; j < shape.verticies.size(); j++) {
        const pair = shape.verticies.get(j); // std_pair_float_float_
        vertices.push({
          x: Number(pair.first),   // convert to JS float
          y: Number(pair.second),  // convert to JS float
        });
      }


      shapes.push({
        id: shape.id,
        ownerId: shape.owner_id,
        type: shape.type,
        position: { x: shape.pos_x, y: shape.pos_y },
        radius: shape.radius,
        size: { x: shape.size_x, y: shape.size_y },
        color: shape.color,
        vertices, // converted array of {x,y}
      });
    }

    console.log("Successful heuristic fetch");
    res.json(shapes);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Native addon failed' });
  }
});

const PORT = 4000;
app.listen(PORT, '0.0.0.0', () => console.log(`Native server running on port ${PORT}`));
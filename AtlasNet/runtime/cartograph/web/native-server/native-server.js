// native-server.js
const express = require('express');
const cors = require('cors');
const addon = require('../nextjs/native/Web.node');
const { HeuristicDraw, IBoundsDrawShape, std_vector_IBoundsDrawShape_ } = addon; // your .node file

function decodeConnectionRow(row) {
  return {
    IdentityId: row[0],
    targetId: row[1],
    pingMs: Number(row[2]),
    inBytesPerSec: Number(row[3]),
    outBytesPerSec: Number(row[4]),
    inPacketsPerSec: Number(row[5]),
    pendingReliableBytes: Number(row[6]),
    pendingUnreliableBytes: row[7],
    sentUnackedReliableBytes: Number(row[8]),
    queueTimeUsec: Number(row[9]),
    qualityLocal: Number(row[10]),
    qualityRemote: Number(row[11]),
    state: row[12],
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

    //const nt = new NetworkTelemetry();

    // SWIG string vector
    const idsVec = new std_vector_std_string_();
    const telemetryVec = new std_vector_std_vector_std_string__();

    nt.GetLivePingIDs(idsVec);
    nt.GetAllTelemetry(telemetryVec);
    const ids = [];
    for (let i = 0; i < idsVec.size(); i++) {
      ids.push(String(idsVec.get(i)));
      //console.log(idsVec.get(i));

    }
    //console.log(ids);

    // Convert telemetryVec: std::vector<std::vector<std::string>> -> string[][]
    const allRows = [];
    for (let i = 0; i < telemetryVec.size(); i++) {
      const rowVec = telemetryVec.get(i);
      const row = [];
      for (let j = 0; j < rowVec.size(); j++) 
      {
        row.push(String(rowVec.get(j)));
        //console.log(String(rowVec.get(j)));
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
    const decoded = decodeConnectionRow(row);
    const shardId = decoded.IdentityId;

    if (!rowsByShard.has(shardId)) {
        rowsByShard.set(shardId, []);
    }
    rowsByShard.get(shardId).push(decoded);
    console.log(`Decoded row for shard ${shardId}:`, decoded);
    }

    // Upload/Download: your C++ methods currently do nothing (commented out),
    // so hardcode here for now OR return null/0 until implemented.
    // For your current UI, you want per-shard speeds, so map ids -> fake speeds.

    const telemetry = ids.map((id, idx) => ({
      shardId: id,
      downloadKbps: 200 + idx * 50, // fake for now
      uploadKbps: 80 + idx * 20,    // fake for now
      connections: rowsByShard.get(id) ?? [],
    }));


    res.json(telemetry);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Native addon failed' });
  }
});

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
                type: shape.type,
                position: { x: shape.pos_x, y: shape.pos_y },
                radius: shape.radius,
                size: { x: shape.size_x, y: shape.size_y },
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

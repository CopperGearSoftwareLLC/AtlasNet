// native-server.js
const express = require('express');
const cors = require('cors');
const addon = require('../nextjs/native/Web.node');
const { HeuristicDraw, IBoundsDrawShape, std_vector_IBoundsDrawShape_ } = addon; // your .node file
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
app.listen(PORT, () => console.log(`Native server running on port ${PORT}`));

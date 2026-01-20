// /app/api/heuristicfetch/route.ts
'use server';

import type { NextRequest } from 'next/server';
import type { ShapeJS } from '../../lib/types';

// Convert raw points to {x, y} array
function convertPoints(points: any): { x: number; y: number }[] {

    if (!points || !Array.isArray(points)) return [];

    const result: { x: number; y: number }[] = [];
    for (let i = 0; i < points.length; i++) {
        const p = points[i];
        // check if it's already an object with x and y
        if (p && typeof p.x === 'number' && typeof p.y === 'number') {
            result.push({ x: p.x, y: p.y });
        } 
        // fallback for array of numbers [x, y]
        else if (Array.isArray(p) && p.length >= 2) {
            result.push({ x: p[0], y: p[1] });
        }
    }
    return result;
}


// Convert raw shape from local server to ShapeJS
function convertShape(shape: any): ShapeJS {
  // start with a default ShapeJS object
  const sjs: ShapeJS = {
    type: 'circle', // temporary default, will be overwritten
    position: { x: shape.position?.x ?? 0, y: shape.position?.y ?? 0 },
  };

  switch (shape.type) {
    case 0: // eCircle
      sjs.type = 'circle';
      sjs.radius = Math.abs(shape.radius ?? 10);
      break;

    case 1: // eRectangle
      sjs.type = 'rectangle';
      sjs.size = { x: shape.size?.x ?? 10, y: shape.size?.y ?? 10 };
      break;

    case 2: // eLine
      sjs.type = 'line';
      sjs.points = convertPoints(shape.vertices);
      break;

    case 3: // ePolygon
      sjs.type = 'polygon';
        //const pointsVector = shape.verticies; // <-- call the getter methode
        //console.log("Polygon with ", shape.points.size(), " Points");
      sjs.points = convertPoints(shape.vertices);
      break;

    case 4: // eRectImage
      sjs.type = 'rectImage';
      sjs.size = { x: shape.size?.x ?? 10, y: shape.size?.y ?? 10 };
      break;

    default:
      console.warn('Unrecognized shape type', shape.type);
      break;
  }

  return sjs;
}


export async function GET(req: NextRequest) {
  try {
        console.log("Fetching shapes Internal");

    // Fetch raw shapes from local backend
    const response = await fetch('http://localhost:4000/heuristic');
    const rawShapes = await response.json();
    console.log("Heuristic raw shapes", rawShapes);
    // Convert to ShapeJS
    const shapes: ShapeJS[] = Array.isArray(rawShapes)
      ? rawShapes.map(convertShape)
      : [];
    console.log("Processed shapes", shapes);
    return new Response(JSON.stringify(shapes), { status: 200 });
  } catch (err) {
    console.error(err);
    return new Response(JSON.stringify({ error: 'Failed to fetch shapes' }), { status: 500 });
  }
}

function readHeuristicShapes(addon) {
  if (!addon || !addon.HeuristicDraw || !addon.std_vector_IBoundsDrawShape_) {
    return [];
  }

  const { HeuristicDraw, std_vector_IBoundsDrawShape_ } = addon;
  const hd = new HeuristicDraw();
  const shapesVector = new std_vector_IBoundsDrawShape_();
  hd.DrawCurrentHeuristic(shapesVector);

  const shapes = [];
  for (let i = 0; i < shapesVector.size(); i += 1) {
    const shape = shapesVector.get(i);
    const vertices = [];
    const halfPlanes = [];
    for (let j = 0; j < shape.verticies.size(); j += 1) {
      const pair = shape.verticies.get(j);
      vertices.push({
        x: Number(pair.first),
        y: Number(pair.second),
      });
    }
    if (shape.half_planes && typeof shape.half_planes.size === 'function') {
      for (let j = 0; j + 2 < shape.half_planes.size(); j += 3) {
        halfPlanes.push({
          nx: Number(shape.half_planes.get(j)),
          ny: Number(shape.half_planes.get(j + 1)),
          c: Number(shape.half_planes.get(j + 2)),
        });
      }
    }

    shapes.push({
      id: shape.id,
      ownerId: shape.owner_id,
      type: shape.type,
      position: { x: shape.pos_x, y: shape.pos_y },
      radius: shape.radius,
      size: { x: shape.size_x, y: shape.size_y },
      color: shape.color,
      vertices,
      halfPlanes,
    });
  }

  return shapes;
}

module.exports = {
  readHeuristicShapes,
};

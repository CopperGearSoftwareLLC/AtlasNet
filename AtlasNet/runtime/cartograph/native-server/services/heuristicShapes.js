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
    for (let j = 0; j < shape.verticies.size(); j += 1) {
      const pair = shape.verticies.get(j);
      vertices.push({
        x: Number(pair.first),
        y: Number(pair.second),
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
      vertices,
    });
  }

  return shapes;
}

module.exports = {
  readHeuristicShapes,
};

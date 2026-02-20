import type { ShapeJS } from './cartographTypes';

export type MapViewMode = '2d' | '3d';
export type MapProjectionMode = 'orthographic' | 'perspective';

interface DrawOptions {
  container: HTMLDivElement;
  shapes: ShapeJS[];
  viewMode?: MapViewMode;
  projectionMode?: MapProjectionMode;
}

interface Vec3 {
  x: number;
  y: number;
  z: number;
}

interface ProjectedPoint {
  x: number;
  y: number;
  depth: number;
}

const MIN_SCALE_2D = 0.05;
const MAX_SCALE_2D = 20;
const MIN_DISTANCE_3D = 5;
const MAX_DISTANCE_3D = 20000;
const MIN_ORTHO_HEIGHT = 1;
const MAX_ORTHO_HEIGHT = 20000;
const MIN_PITCH = -Math.PI / 2 + 0.02;
const MAX_PITCH = Math.PI / 2 - 0.02;
const DEFAULT_FOV_DEG = 60;
const NEAR_PLANE = 0.1;
const CAMERA_ORBIT_SPEED = 0.006;
const CAMERA_PAN_SCALE = 0.0022;

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function vecAdd(a: Vec3, b: Vec3): Vec3 {
  return { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z };
}

function vecSub(a: Vec3, b: Vec3): Vec3 {
  return { x: a.x - b.x, y: a.y - b.y, z: a.z - b.z };
}

function vecScale(a: Vec3, scalar: number): Vec3 {
  return { x: a.x * scalar, y: a.y * scalar, z: a.z * scalar };
}

function vecDot(a: Vec3, b: Vec3): number {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

function vecCross(a: Vec3, b: Vec3): Vec3 {
  return {
    x: a.y * b.z - a.z * b.y,
    y: a.z * b.x - a.x * b.z,
    z: a.x * b.y - a.y * b.x,
  };
}

function vecLength(a: Vec3): number {
  return Math.hypot(a.x, a.y, a.z);
}

function vecNormalize(a: Vec3): Vec3 {
  const len = vecLength(a);
  if (len <= 1e-8) {
    return { x: 0, y: 0, z: 0 };
  }
  return vecScale(a, 1 / len);
}

function mapPointToWorld(point: { x: number; y: number; z?: number }): Vec3 {
  return {
    x: Number(point.x ?? 0),
    y: Number(point.z ?? 0),
    z: Number(point.y ?? 0),
  };
}

function shapePositionToWorld(shape: ShapeJS): Vec3 {
  return mapPointToWorld(shape.position as { x: number; y: number; z?: number });
}

function niceStep(rawStep: number): number {
  const magnitude = Math.pow(10, Math.floor(Math.log10(Math.max(rawStep, 1e-6))));
  const residual = rawStep / magnitude;
  if (residual >= 5) return 5 * magnitude;
  if (residual >= 2) return 2 * magnitude;
  return magnitude;
}

function parseRgbaColor(input: string): [number, number, number, number] | null {
  const value = input.trim().toLowerCase();
  if (value.startsWith('#')) {
    if (value.length === 7) {
      const r = Number.parseInt(value.slice(1, 3), 16);
      const g = Number.parseInt(value.slice(3, 5), 16);
      const b = Number.parseInt(value.slice(5, 7), 16);
      return [r, g, b, 1];
    }
    return null;
  }

  const rgbaMatch = value.match(
    /^rgba?\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)(?:\s*,\s*([0-9.]+))?\s*\)$/
  );
  if (!rgbaMatch) {
    return null;
  }

  const r = clamp(Number(rgbaMatch[1]), 0, 255);
  const g = clamp(Number(rgbaMatch[2]), 0, 255);
  const b = clamp(Number(rgbaMatch[3]), 0, 255);
  const a = rgbaMatch[4] == null ? 1 : clamp(Number(rgbaMatch[4]), 0, 1);
  return [r, g, b, a];
}

function darkenColor(color: string, factor: number): string {
  const parsed = parseRgbaColor(color);
  if (!parsed) {
    return color;
  }
  const [r, g, b, a] = parsed;
  return `rgba(${Math.round(r * factor)}, ${Math.round(g * factor)}, ${Math.round(
    b * factor
  )}, ${a})`;
}

export function createMapRenderer({
  container,
  shapes: initialShapes = [],
  viewMode: initialViewMode = '2d',
  projectionMode: initialProjectionMode = 'orthographic',
}: DrawOptions) {
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    throw new Error('Failed to create map renderer context.');
  }
  canvas.style.touchAction = 'none';
  canvas.style.userSelect = 'none';
  canvas.tabIndex = 0;
  container.appendChild(canvas);

  let shapes = initialShapes;
  let viewMode: MapViewMode = initialViewMode;
  let projectionMode: MapProjectionMode = initialProjectionMode;

  // 2D camera state
  let offsetX = 0;
  let offsetY = 0;
  let scale2D = 1;

  // 3D camera state (Unity-like orbit camera)
  let cameraTarget: Vec3 = { x: 0, y: 0, z: 0 };
  let cameraDistance = 180;
  let cameraYaw = Math.PI / 4;
  let cameraPitch = 0.75;
  let orthoHeight = 120;
  const fovRad = (DEFAULT_FOV_DEG * Math.PI) / 180;

  let pointerDragMode: 'none' | 'pan2d' | 'orbit3d' | 'pan3d' = 'none';
  let activePointerId: number | null = null;
  let lastPointerX = 0;
  let lastPointerY = 0;
  let didAutoFrame = false;

  function canvasWidth(): number {
    return canvas.width || 1;
  }

  function canvasHeight(): number {
    return canvas.height || 1;
  }

  function getCameraBasis(): {
    position: Vec3;
    forward: Vec3;
    right: Vec3;
    up: Vec3;
  } {
    const cosPitch = Math.cos(cameraPitch);
    const sinPitch = Math.sin(cameraPitch);
    const sinYaw = Math.sin(cameraYaw);
    const cosYaw = Math.cos(cameraYaw);

    const position = {
      x: cameraTarget.x + cameraDistance * cosPitch * sinYaw,
      y: cameraTarget.y + cameraDistance * sinPitch,
      z: cameraTarget.z + cameraDistance * cosPitch * cosYaw,
    };

    const forward = vecNormalize(vecSub(cameraTarget, position));
    let right = vecNormalize(vecCross(forward, { x: 0, y: 1, z: 0 }));
    if (vecLength(right) < 1e-5) {
      right = { x: 1, y: 0, z: 0 };
    }
    const up = vecNormalize(vecCross(right, forward));
    return { position, forward, right, up };
  }

  function project3D(point: Vec3): ProjectedPoint | null {
    const basis = getCameraBasis();
    const rel = vecSub(point, basis.position);

    const cx = vecDot(rel, basis.right);
    const cy = vecDot(rel, basis.up);
    const cz = vecDot(rel, basis.forward);

    if (cz <= NEAR_PLANE) {
      return null;
    }

    const aspect = canvasWidth() / canvasHeight();
    let ndcX = 0;
    let ndcY = 0;

    if (projectionMode === 'perspective') {
      const f = 1 / Math.tan(fovRad / 2);
      ndcX = (cx * f) / (cz * aspect);
      ndcY = (cy * f) / cz;
    } else {
      const halfHeight = Math.max(MIN_ORTHO_HEIGHT, orthoHeight);
      const halfWidth = halfHeight * aspect;
      ndcX = cx / halfWidth;
      ndcY = cy / halfHeight;
    }

    return {
      x: (ndcX * 0.5 + 0.5) * canvasWidth(),
      y: (1 - (ndcY * 0.5 + 0.5)) * canvasHeight(),
      depth: cz,
    };
  }

  function drawLine3D(a: Vec3, b: Vec3, color: string, width = 1.2): void {
    const pa = project3D(a);
    const pb = project3D(b);
    if (!pa || !pb) {
      return;
    }
    ctx.beginPath();
    ctx.moveTo(pa.x, pa.y);
    ctx.lineTo(pb.x, pb.y);
    ctx.strokeStyle = color;
    ctx.lineWidth = width;
    ctx.stroke();
  }

  function drawPolyline3D(points: Vec3[], closed: boolean, color: string, width = 1.4): void {
    if (points.length < 2) {
      return;
    }

    const projected: Array<ProjectedPoint | null> = points.map(project3D);
    let started = false;

    ctx.beginPath();
    for (let i = 0; i < projected.length; i += 1) {
      const p = projected[i];
      if (!p) {
        started = false;
        continue;
      }
      if (!started) {
        ctx.moveTo(p.x, p.y);
        started = true;
      } else {
        ctx.lineTo(p.x, p.y);
      }
    }

    if (closed) {
      const first = projected[0];
      const last = projected[projected.length - 1];
      if (first && last) {
        ctx.lineTo(first.x, first.y);
      }
    }

    ctx.strokeStyle = color;
    ctx.lineWidth = width;
    ctx.stroke();
  }

  function drawAxes2D(): void {
    const minPixelSpacing = 50;
    const fontSize = 12;
    ctx.save();

    ctx.strokeStyle = 'rgba(100, 116, 139, 0.55)';
    ctx.fillStyle = '#d6deec';
    ctx.lineWidth = 1;
    ctx.font = `${fontSize}px sans-serif`;

    const step = niceStep(minPixelSpacing / scale2D);
    const startX = -offsetX / scale2D;
    const endX = (canvasWidth() - offsetX) / scale2D;
    const startY = -offsetY / scale2D;
    const endY = (canvasHeight() - offsetY) / scale2D;

    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    for (let x = Math.floor(startX / step) * step; x <= endX; x += step) {
      const screenX = x * scale2D + offsetX;
      ctx.beginPath();
      ctx.moveTo(screenX, 0);
      ctx.lineTo(screenX, canvasHeight());
      ctx.stroke();
      ctx.save();
      ctx.scale(1, -1);
      ctx.fillText(x.toFixed(0), screenX, -2);
      ctx.restore();
    }

    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (let y = Math.floor(startY / step) * step; y <= endY; y += step) {
      const screenY = y * scale2D + offsetY;
      ctx.beginPath();
      ctx.moveTo(0, screenY);
      ctx.lineTo(canvasWidth(), screenY);
      ctx.stroke();
      ctx.save();
      ctx.scale(1, -1);
      ctx.fillText(y.toFixed(0), canvasWidth() - 2, -screenY);
      ctx.restore();
    }

    ctx.restore();
  }

  function drawAxes3D(): void {
    const basis = getCameraBasis();
    const gridCenter = {
      x: cameraTarget.x,
      y: 0,
      z: cameraTarget.z,
    };
    const gridScaleRef =
      projectionMode === 'perspective'
        ? cameraDistance
        : Math.max(MIN_ORTHO_HEIGHT, orthoHeight) * 1.6;
    const step = niceStep(Math.max(2, gridScaleRef / 10));
    const halfExtent = step * 14;

    ctx.lineWidth = 1;
    for (let x = -halfExtent; x <= halfExtent; x += step) {
      const worldX = gridCenter.x + x;
      drawLine3D(
        { x: worldX, y: 0, z: gridCenter.z - halfExtent },
        { x: worldX, y: 0, z: gridCenter.z + halfExtent },
        Math.abs(x) < 1e-6
          ? 'rgba(239, 68, 68, 0.8)'
          : 'rgba(148, 163, 184, 0.25)',
        Math.abs(x) < 1e-6 ? 1.8 : 1
      );
    }

    for (let z = -halfExtent; z <= halfExtent; z += step) {
      const worldZ = gridCenter.z + z;
      drawLine3D(
        { x: gridCenter.x - halfExtent, y: 0, z: worldZ },
        { x: gridCenter.x + halfExtent, y: 0, z: worldZ },
        Math.abs(z) < 1e-6
          ? 'rgba(59, 130, 246, 0.8)'
          : 'rgba(148, 163, 184, 0.25)',
        Math.abs(z) < 1e-6 ? 1.8 : 1
      );
    }

    // Camera target indicator
    const projectedTarget = project3D(cameraTarget);
    if (projectedTarget) {
      ctx.beginPath();
      ctx.arc(projectedTarget.x, projectedTarget.y, 3.5, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(251, 191, 36, 0.9)';
      ctx.fill();
      ctx.strokeStyle = 'rgba(30, 41, 59, 0.85)';
      ctx.stroke();
    }

    // Draw simple axis helper in top-left
    const axisOrigin = { x: 54, y: 54 };
    const axisSize = 24;
    const axisRight = basis.right;
    const axisUp = basis.up;
    const axisForward = basis.forward;

    const axis2D = (v: Vec3) => ({
      x: axisOrigin.x + (v.x * axisRight.x + v.y * axisRight.y + v.z * axisRight.z) * axisSize,
      y: axisOrigin.y - (v.x * axisUp.x + v.y * axisUp.y + v.z * axisUp.z) * axisSize,
    });

    const xEnd = axis2D({ x: 1, y: 0, z: 0 });
    const yEnd = axis2D({ x: 0, y: 1, z: 0 });
    const zEnd = axis2D({ x: 0, y: 0, z: 1 });

    ctx.lineWidth = 2;
    ctx.strokeStyle = 'rgba(239, 68, 68, 0.9)';
    ctx.beginPath();
    ctx.moveTo(axisOrigin.x, axisOrigin.y);
    ctx.lineTo(xEnd.x, xEnd.y);
    ctx.stroke();

    ctx.strokeStyle = 'rgba(34, 197, 94, 0.9)';
    ctx.beginPath();
    ctx.moveTo(axisOrigin.x, axisOrigin.y);
    ctx.lineTo(yEnd.x, yEnd.y);
    ctx.stroke();

    ctx.strokeStyle = 'rgba(59, 130, 246, 0.9)';
    ctx.beginPath();
    ctx.moveTo(axisOrigin.x, axisOrigin.y);
    ctx.lineTo(zEnd.x, zEnd.y);
    ctx.stroke();

    const facing = axisForward.y > 0 ? 'Top' : 'Bottom';
    ctx.fillStyle = 'rgba(226, 232, 240, 0.9)';
    ctx.font = '11px sans-serif';
    ctx.fillText(facing, axisOrigin.x + 16, axisOrigin.y + 22);
  }

  function drawShapes2D(): void {
    if (!shapes.length) {
      return;
    }

    for (const shape of shapes) {
      const x = shape.position.x * scale2D + offsetX;
      const y = shape.position.y * scale2D + offsetY;
      const color = shape.color ?? 'rgba(100, 149, 255, 1)';

      ctx.save();
      ctx.translate(x, y);
      ctx.scale(scale2D, scale2D);

      switch (shape.type) {
        case 'circle': {
          ctx.beginPath();
          ctx.arc(0, 0, shape.radius ?? 10, 0, Math.PI * 2);
          ctx.strokeStyle = color;
          ctx.stroke();
          break;
        }
        case 'rectangle':
        case 'rectImage': {
          const w = shape.size?.x ?? 10;
          const h = shape.size?.y ?? 10;
          ctx.strokeStyle = color;
          ctx.strokeRect(-w / 2, -h / 2, w, h);
          break;
        }
        case 'polygon': {
          const pts = shape.points ?? [];
          if (pts.length > 0) {
            ctx.beginPath();
            ctx.moveTo(pts[0].x, pts[0].y);
            for (let i = 1; i < pts.length; i += 1) {
              ctx.lineTo(pts[i].x, pts[i].y);
            }
            ctx.closePath();
            ctx.strokeStyle = color;
            ctx.stroke();
          }
          break;
        }
        case 'line': {
          const pts = shape.points ?? [];
          if (pts.length >= 2) {
            ctx.restore();
            ctx.beginPath();
            ctx.moveTo(pts[0].x * scale2D + offsetX, pts[0].y * scale2D + offsetY);
            for (let i = 1; i < pts.length; i += 1) {
              ctx.lineTo(pts[i].x * scale2D + offsetX, pts[i].y * scale2D + offsetY);
            }
            ctx.strokeStyle = color;
            ctx.stroke();
            continue;
          }
          break;
        }
      }
      ctx.restore();
    }
  }

  function drawShapes3D(): void {
    if (!shapes.length) {
      return;
    }

    for (const shape of shapes) {
      const base = shapePositionToWorld(shape);
      const color = shape.color ?? 'rgba(100, 149, 255, 1)';
      const fillColor = darkenColor(color, 0.55);

      switch (shape.type) {
        case 'line': {
          const pts = shape.points ?? [];
          if (pts.length >= 2) {
            const worldPts = pts.map((p) => mapPointToWorld(p as { x: number; y: number; z?: number }));
            drawPolyline3D(worldPts, false, color, 1.4);
          }
          break;
        }
        case 'circle': {
          const radius = Math.max(0.001, Math.abs(shape.radius ?? 1));
          const segments = Math.max(16, Math.min(64, Math.round(radius * 2.5)));
          const ring: Vec3[] = [];
          for (let i = 0; i <= segments; i += 1) {
            const angle = (i / segments) * Math.PI * 2;
            ring.push({
              x: base.x + Math.cos(angle) * radius,
              y: base.y,
              z: base.z + Math.sin(angle) * radius,
            });
          }
          drawPolyline3D(ring, false, color, 1.5);
          break;
        }
        case 'rectangle':
        case 'rectImage': {
          const w = Math.abs(shape.size?.x ?? 1);
          const h = Math.abs(shape.size?.y ?? 1);
          const halfW = w / 2;
          const halfH = h / 2;
          const corners: Vec3[] = [
            { x: base.x - halfW, y: base.y, z: base.z - halfH },
            { x: base.x + halfW, y: base.y, z: base.z - halfH },
            { x: base.x + halfW, y: base.y, z: base.z + halfH },
            { x: base.x - halfW, y: base.y, z: base.z + halfH },
          ];
          drawPolyline3D(corners, true, color, 1.5);
          break;
        }
        case 'polygon': {
          const localPts = shape.points ?? [];
          if (localPts.length >= 2) {
            const worldPts = localPts.map((p) => ({
              x: base.x + (p.x ?? 0),
              y: base.y + ((p as { z?: number }).z ?? 0),
              z: base.z + (p.y ?? 0),
            }));
            drawPolyline3D(worldPts, true, color, 1.5);
          }
          break;
        }
      }

      // Simple vertical cue for entities/shard dots in 3D mode.
      if (shape.type === 'circle' && (shape.radius ?? 0) <= 3.2) {
        drawLine3D(
          { x: base.x, y: base.y, z: base.z },
          { x: base.x, y: base.y + Math.max(0.8, (shape.radius ?? 1) * 1.2), z: base.z },
          fillColor,
          1
        );
      }
    }
  }

  function draw(): void {
    ctx.clearRect(0, 0, canvasWidth(), canvasHeight());

    if (viewMode === '2d') {
      ctx.save();
      ctx.translate(0, canvasHeight());
      ctx.scale(1, -1);
      drawAxes2D();
      drawShapes2D();
      ctx.restore();
      return;
    }

    // 3D
    drawAxes3D();
    drawShapes3D();
  }

  function computeBoundsFromShapes(): {
    minX: number;
    maxX: number;
    minZ: number;
    maxZ: number;
  } | null {
    if (!shapes.length) {
      return null;
    }

    let minX = Infinity;
    let maxX = -Infinity;
    let minZ = Infinity;
    let maxZ = -Infinity;

    function includePoint(x: number, z: number): void {
      if (!Number.isFinite(x) || !Number.isFinite(z)) {
        return;
      }
      minX = Math.min(minX, x);
      maxX = Math.max(maxX, x);
      minZ = Math.min(minZ, z);
      maxZ = Math.max(maxZ, z);
    }

    for (const shape of shapes) {
      const base = shapePositionToWorld(shape);
      includePoint(base.x, base.z);

      if (shape.type === 'line' && shape.points) {
        for (const p of shape.points) {
          includePoint(p.x, p.y);
        }
      }

      if (shape.type === 'polygon' && shape.points) {
        for (const p of shape.points) {
          includePoint(base.x + p.x, base.z + p.y);
        }
      }

      if (shape.type === 'rectangle' || shape.type === 'rectImage') {
        const w = Math.abs(shape.size?.x ?? 0);
        const h = Math.abs(shape.size?.y ?? 0);
        includePoint(base.x - w / 2, base.z - h / 2);
        includePoint(base.x + w / 2, base.z + h / 2);
      }

      if (shape.type === 'circle') {
        const radius = Math.abs(shape.radius ?? 0);
        includePoint(base.x - radius, base.z - radius);
        includePoint(base.x + radius, base.z + radius);
      }
    }

    if (!Number.isFinite(minX) || !Number.isFinite(minZ)) {
      return null;
    }

    return { minX, maxX, minZ, maxZ };
  }

  function resetCamera(): void {
    const bounds = computeBoundsFromShapes();
    if (!bounds) {
      offsetX = canvasWidth() / 2;
      offsetY = canvasHeight() / 2;
      scale2D = 1;
      cameraTarget = { x: 0, y: 0, z: 0 };
      cameraDistance = 180;
      orthoHeight = 120;
      cameraYaw = Math.PI / 4;
      cameraPitch = 0.75;
      draw();
      return;
    }

    const centerX = (bounds.minX + bounds.maxX) / 2;
    const centerZ = (bounds.minZ + bounds.maxZ) / 2;
    const sizeX = Math.max(10, bounds.maxX - bounds.minX);
    const sizeZ = Math.max(10, bounds.maxZ - bounds.minZ);
    const extent = Math.max(sizeX, sizeZ);

    cameraTarget = { x: centerX, y: 0, z: centerZ };
    cameraDistance = clamp(extent * 1.8, 45, MAX_DISTANCE_3D);
    orthoHeight = clamp(extent * 0.85, 12, MAX_ORTHO_HEIGHT);
    cameraYaw = Math.PI / 4;
    cameraPitch = 0.75;

    scale2D = clamp(Math.min(canvasWidth(), canvasHeight()) / (extent * 1.4), MIN_SCALE_2D, MAX_SCALE_2D);
    offsetX = canvasWidth() / 2 - centerX * scale2D;
    offsetY = canvasHeight() / 2 - centerZ * scale2D;

    draw();
  }

  function ensureAutoFrame(): void {
    if (didAutoFrame || shapes.length === 0) {
      return;
    }
    didAutoFrame = true;
    resetCamera();
  }

  function resizeCanvas(): void {
    canvas.width = Math.max(1, container.clientWidth);
    canvas.height = Math.max(1, container.clientHeight);
    draw();
  }

  function onPointerDown(event: PointerEvent): void {
    canvas.focus();
    activePointerId = event.pointerId;
    lastPointerX = event.clientX;
    lastPointerY = event.clientY;

    if (viewMode === '2d') {
      pointerDragMode = 'pan2d';
      canvas.setPointerCapture(event.pointerId);
      return;
    }

    const orbitRequested = event.button === 2 || (event.button === 0 && event.altKey);
    const panRequested = event.button === 1;

    if (orbitRequested) {
      pointerDragMode = 'orbit3d';
      canvas.setPointerCapture(event.pointerId);
      event.preventDefault();
      return;
    }

    if (panRequested) {
      pointerDragMode = 'pan3d';
      canvas.setPointerCapture(event.pointerId);
      event.preventDefault();
    }
  }

  function onPointerMove(event: PointerEvent): void {
    if (activePointerId !== event.pointerId || pointerDragMode === 'none') {
      return;
    }

    const dx = event.clientX - lastPointerX;
    const dy = event.clientY - lastPointerY;
    lastPointerX = event.clientX;
    lastPointerY = event.clientY;

    if (pointerDragMode === 'pan2d') {
      offsetX += dx;
      offsetY -= dy;
      draw();
      return;
    }

    if (pointerDragMode === 'orbit3d') {
      cameraYaw -= dx * CAMERA_ORBIT_SPEED;
      cameraPitch = clamp(cameraPitch - dy * CAMERA_ORBIT_SPEED, MIN_PITCH, MAX_PITCH);
      draw();
      return;
    }

    if (pointerDragMode === 'pan3d') {
      const basis = getCameraBasis();
      const panMagnitude =
        projectionMode === 'perspective'
          ? cameraDistance * CAMERA_PAN_SCALE
          : Math.max(MIN_ORTHO_HEIGHT, orthoHeight) * CAMERA_PAN_SCALE;

      const panRight = vecScale(basis.right, -dx * panMagnitude);
      const panUp = vecScale(basis.up, dy * panMagnitude);
      cameraTarget = vecAdd(cameraTarget, vecAdd(panRight, panUp));
      draw();
    }
  }

  function endPointerDrag(event: PointerEvent): void {
    if (activePointerId !== event.pointerId) {
      return;
    }
    pointerDragMode = 'none';
    activePointerId = null;
    try {
      canvas.releasePointerCapture(event.pointerId);
    } catch {}
  }

  function onWheel(event: WheelEvent): void {
    event.preventDefault();
    const zoomOutFactor = 1.1;
    const zoomInFactor = 1 / zoomOutFactor;
    const zoomFactor = event.deltaY > 0 ? zoomOutFactor : zoomInFactor;

    if (viewMode === '2d') {
      const mouseX = event.offsetX;
      const mouseY = event.offsetY;
      const nextScale = clamp(scale2D / zoomFactor, MIN_SCALE_2D, MAX_SCALE_2D);

      offsetX = mouseX - ((mouseX - offsetX) * nextScale) / scale2D;
      offsetY = mouseY - ((mouseY - offsetY) * nextScale) / scale2D;
      scale2D = nextScale;
      draw();
      return;
    }

    if (projectionMode === 'perspective') {
      cameraDistance = clamp(cameraDistance * zoomFactor, MIN_DISTANCE_3D, MAX_DISTANCE_3D);
    } else {
      orthoHeight = clamp(orthoHeight * zoomFactor, MIN_ORTHO_HEIGHT, MAX_ORTHO_HEIGHT);
    }
    draw();
  }

  function onContextMenu(event: MouseEvent): void {
    event.preventDefault();
  }

  function onKeyDown(event: KeyboardEvent): void {
    if (event.key.toLowerCase() === 'f') {
      resetCamera();
      event.preventDefault();
    }
  }

  window.addEventListener('resize', resizeCanvas);
  canvas.addEventListener('pointerdown', onPointerDown);
  canvas.addEventListener('pointermove', onPointerMove);
  canvas.addEventListener('pointerup', endPointerDrag);
  canvas.addEventListener('pointercancel', endPointerDrag);
  canvas.addEventListener('wheel', onWheel, { passive: false });
  canvas.addEventListener('contextmenu', onContextMenu);
  canvas.addEventListener('keydown', onKeyDown);

  resizeCanvas();
  ensureAutoFrame();
  draw();

  return {
    draw,
    setShapes(newShapes: ShapeJS[]) {
      shapes = newShapes;
      ensureAutoFrame();
      draw();
    },
    setOffset(newOffsetX: number, newOffsetY: number) {
      offsetX = newOffsetX;
      offsetY = newOffsetY;
      draw();
    },
    setScale(newScale: number) {
      scale2D = clamp(newScale, MIN_SCALE_2D, MAX_SCALE_2D);
      draw();
    },
    setViewMode(nextViewMode: MapViewMode) {
      viewMode = nextViewMode;
      draw();
    },
    setProjectionMode(nextProjectionMode: MapProjectionMode) {
      projectionMode = nextProjectionMode;
      draw();
    },
    resetCamera,
    destroy() {
      window.removeEventListener('resize', resizeCanvas);
      canvas.removeEventListener('pointerdown', onPointerDown);
      canvas.removeEventListener('pointermove', onPointerMove);
      canvas.removeEventListener('pointerup', endPointerDrag);
      canvas.removeEventListener('pointercancel', endPointerDrag);
      canvas.removeEventListener('wheel', onWheel);
      canvas.removeEventListener('contextmenu', onContextMenu);
      canvas.removeEventListener('keydown', onKeyDown);
      if (canvas.parentElement === container) {
        container.removeChild(canvas);
      }
    },
  };
}

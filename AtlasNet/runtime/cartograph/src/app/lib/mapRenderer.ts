import type { ShapeJS } from './cartographTypes';

export type MapViewMode = '2d' | '3d';
export type MapProjectionMode = 'orthographic' | 'perspective';
export type MapViewPreset = 'iso' | 'top' | 'front' | 'right';

interface DrawOptions {
  container: HTMLDivElement;
  shapes: ShapeJS[];
  viewMode?: MapViewMode;
  projectionMode?: MapProjectionMode;
  interactionSensitivity?: number;
  onPointerWorldPosition?: (
    point: { x: number; y: number } | null,
    screen: { x: number; y: number } | null
  ) => void;
}

interface EdgeLabelOverlay {
  from: { x: number; y: number };
  to: { x: number; y: number };
  text: string;
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

interface MapPoint {
  x: number;
  y: number;
}

interface EntityFocusOverlay {
  enabled: boolean;
  selectedPoints: MapPoint[];
  inspectedPoint: MapPoint | null;
  hoveredPoint: MapPoint | null;
}

const MIN_SCALE_2D = 0.05;
const MAX_SCALE_2D = 20;
const DEFAULT_FAR_PLANE_3D = 1_000_000;
const DEFAULT_FOV_DEG = 60;
const NEAR_PLANE = 0.1;
const CAMERA_ORBIT_SPEED = 0.0035;
const CAMERA_PAN_SCALE = 0.0022;
const GRID_LINE_COUNT_PER_SIDE = 14;
const CAMERA_FLY_BASE_SPEED = 90;
const CAMERA_FLY_SHIFT_MULTIPLIER = 2.5;
const MIN_INTERACTION_SENSITIVITY = 0;
const MIN_PITCH = -Math.PI / 2 + 0.02;
const MAX_PITCH = Math.PI / 2 - 0.02;
const AUTHORITY_ENTITY_WORLD_RADIUS = 1.8;

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function normalizeInteractionSensitivity(
  value: number,
  fallback: number
): number {
  const numeric = Number.isFinite(value) ? value : fallback;
  return Math.max(MIN_INTERACTION_SENSITIVITY, numeric);
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

function vecLerp(a: Vec3, b: Vec3, t: number): Vec3 {
  return {
    x: a.x + (b.x - a.x) * t,
    y: a.y + (b.y - a.y) * t,
    z: a.z + (b.z - a.z) * t,
  };
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

function getOrbitOffset(distance: number, yaw: number, pitch: number): Vec3 {
  const cosPitch = Math.cos(pitch);
  return {
    x: distance * cosPitch * Math.sin(yaw),
    y: distance * Math.sin(pitch),
    z: distance * cosPitch * Math.cos(yaw),
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
  interactionSensitivity: initialInteractionSensitivity = 1,
  onPointerWorldPosition: initialOnPointerWorldPosition,
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
  let cameraRoll = 0;
  let orthoHeight = 120;
  const fovRad = (DEFAULT_FOV_DEG * Math.PI) / 180;

  let pointerDragMode: 'none' | 'pan2d' | 'orbit3d' | 'pan3d' = 'none';
  let activePointerId: number | null = null;
  let lastPointerX = 0;
  let lastPointerY = 0;
  let didAutoFrame = false;
  const flyKeys = new Set<string>();
  let flyRafId: number | null = null;
  let lastFlyFrameMs: number | null = null;
  let interactionSensitivity = normalizeInteractionSensitivity(
    initialInteractionSensitivity,
    1
  );
  let onPointerWorldPosition = initialOnPointerWorldPosition;
  let hoverEdgeLabels: EdgeLabelOverlay[] = [];
  let lastPointerScreen: { x: number; y: number } | null = null;
  let entityFocusOverlay: EntityFocusOverlay = {
    enabled: false,
    selectedPoints: [],
    inspectedPoint: null,
    hoveredPoint: null,
  };

  function canvasWidth(): number {
    return canvas.width || 1;
  }

  function canvasHeight(): number {
    return canvas.height || 1;
  }

  function worldToScreen2D(worldX: number, worldY: number): { x: number; y: number } {
    const baseX = worldX * scale2D + offsetX;
    const baseY = worldY * scale2D + offsetY;
    const centerX = canvasWidth() / 2;
    const centerY = canvasHeight() / 2;
    const dx = baseX - centerX;
    const dy = baseY - centerY;
    const rollCos = Math.cos(cameraRoll);
    const rollSin = Math.sin(cameraRoll);
    const rotatedX = centerX + dx * rollCos - dy * rollSin;
    const rotatedY = centerY + dx * rollSin + dy * rollCos;
    return {
      x: rotatedX,
      y: canvasHeight() - rotatedY,
    };
  }

  function screenToWorld2D(screenX: number, screenY: number): { x: number; y: number } {
    const centerX = canvasWidth() / 2;
    const centerY = canvasHeight() / 2;
    const rotatedX = screenX;
    const rotatedY = canvasHeight() - screenY;
    const dxRot = rotatedX - centerX;
    const dyRot = rotatedY - centerY;
    const rollCos = Math.cos(cameraRoll);
    const rollSin = Math.sin(cameraRoll);
    const dx = dxRot * rollCos + dyRot * rollSin;
    const dy = -dxRot * rollSin + dyRot * rollCos;
    const baseX = centerX + dx;
    const baseY = centerY + dy;
    return {
      x: (baseX - offsetX) / Math.max(scale2D, 1e-6),
      y: (baseY - offsetY) / Math.max(scale2D, 1e-6),
    };
  }

  function screenToWorld3D(screenX: number, screenY: number): { x: number; y: number } | null {
    const width = canvasWidth();
    const height = canvasHeight();
    const ndcX = (screenX / width) * 2 - 1;
    const ndcY = 1 - (screenY / height) * 2;
    const aspect = width / height;
    const basis = getCameraBasis();

    let rayOrigin = basis.position;
    let rayDirection = basis.forward;

    if (projectionMode === 'perspective') {
      const f = 1 / Math.tan(fovRad / 2);
      const dirCamera = vecNormalize({
        x: (ndcX * aspect) / f,
        y: ndcY / f,
        z: 1,
      });
      rayDirection = vecNormalize(
        vecAdd(
          vecAdd(
            vecScale(basis.right, dirCamera.x),
            vecScale(basis.up, dirCamera.y)
          ),
          vecScale(basis.forward, dirCamera.z)
        )
      );
    } else {
      const halfHeight = Math.max(Math.abs(orthoHeight), 1e-8);
      const halfWidth = halfHeight * aspect;
      const orthoOffset = vecAdd(
        vecScale(basis.right, ndcX * halfWidth),
        vecScale(basis.up, ndcY * halfHeight)
      );
      rayOrigin = vecAdd(basis.position, orthoOffset);
      rayDirection = basis.forward;
    }

    if (Math.abs(rayDirection.y) < 1e-6) {
      return null;
    }

    const t = -rayOrigin.y / rayDirection.y;
    if (!Number.isFinite(t) || t < 0) {
      return null;
    }

    const hitPoint = vecAdd(rayOrigin, vecScale(rayDirection, t));
    return { x: hitPoint.x, y: hitPoint.z };
  }

  function screenToWorldMap(screenX: number, screenY: number): { x: number; y: number } | null {
    if (viewMode === '2d') {
      return screenToWorld2D(screenX, screenY);
    }
    return screenToWorld3D(screenX, screenY);
  }

  function projectMapPoint3D(
    point: { x: number; y: number },
    basis: CameraBasis
  ): ProjectedPoint | null {
    const cameraPoint = worldToCamera(
      mapPointToWorld({ x: point.x, y: point.y }),
      basis
    );
    if (projectionMode === 'perspective' && cameraPoint.z <= NEAR_PLANE) {
      return null;
    }
    return projectCameraPoint(cameraPoint);
  }

  function projectMapPointToScreen(
    point: MapPoint,
    basisOverride?: CameraBasis
  ): { x: number; y: number } | null {
    if (viewMode === '2d') {
      return worldToScreen2D(point.x, point.y);
    }

    const projected = projectMapPoint3D(point, basisOverride ?? getCameraBasis());
    if (!projected) {
      return null;
    }
    return { x: projected.x, y: projected.y };
  }

  function getEntityFocusRadius3D(
    point: MapPoint,
    basis: CameraBasis
  ): number {
    const center = projectMapPoint3D(point, basis);
    if (!center) {
      return 6;
    }

    const sampleX = projectMapPoint3D(
      { x: point.x + AUTHORITY_ENTITY_WORLD_RADIUS, y: point.y },
      basis
    );
    const sampleY = projectMapPoint3D(
      { x: point.x, y: point.y + AUTHORITY_ENTITY_WORLD_RADIUS },
      basis
    );

    let projectedRadius = 0;
    if (sampleX) {
      projectedRadius = Math.max(
        projectedRadius,
        Math.hypot(sampleX.x - center.x, sampleX.y - center.y)
      );
    }
    if (sampleY) {
      projectedRadius = Math.max(
        projectedRadius,
        Math.hypot(sampleY.x - center.x, sampleY.y - center.y)
      );
    }

    if (!Number.isFinite(projectedRadius) || projectedRadius <= 0.001) {
      return 6;
    }

    return clamp(projectedRadius, 1.5, 72);
  }

  function drawEntityFocusMarker(
    point: MapPoint,
    screenX: number,
    screenY: number,
    tone: 'selected' | 'focus',
    basis?: CameraBasis
  ): void {
    const isFocus = tone === 'focus';
    const outerRadius =
      viewMode === '2d'
        ? AUTHORITY_ENTITY_WORLD_RADIUS * Math.max(scale2D, 0.0001)
        : getEntityFocusRadius3D(point, basis ?? getCameraBasis());
    const innerRadius = outerRadius * 0.55;
    const strokeWidth =
      viewMode === '2d'
        ? Math.max(scale2D, 0.0001)
        : clamp(outerRadius * 0.2, 1.4, 3.2);
    const strokeColor = isFocus
      ? 'rgba(59, 130, 246, 0.95)'
      : 'rgba(251, 191, 36, 0.95)';
    const fillColor = isFocus
      ? 'rgba(59, 130, 246, 0.36)'
      : 'rgba(251, 191, 36, 0.3)';

    ctx.save();
    ctx.beginPath();
    ctx.arc(screenX, screenY, outerRadius, 0, Math.PI * 2);
    ctx.fillStyle = fillColor;
    ctx.fill();
    ctx.lineWidth = strokeWidth;
    ctx.strokeStyle = strokeColor;
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(screenX, screenY, innerRadius, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
    ctx.fill();
    ctx.restore();
  }

  function drawEntityFocusOverlay(): void {
    if (!entityFocusOverlay.enabled) {
      return;
    }

    ctx.save();
    ctx.fillStyle = 'rgba(2, 6, 23, 0.58)';
    ctx.fillRect(0, 0, canvasWidth(), canvasHeight());
    ctx.restore();

    const basis = viewMode === '3d' ? getCameraBasis() : undefined;

    const projectedFocusKeys = new Set<string>();
    const projectKeyForPoint = (point: MapPoint): string =>
      `${point.x.toFixed(6)}:${point.y.toFixed(6)}`;

    for (const point of entityFocusOverlay.selectedPoints) {
      const screen = projectMapPointToScreen(point, basis);
      if (!screen) {
        continue;
      }
      drawEntityFocusMarker(point, screen.x, screen.y, 'selected', basis);
    }

    if (entityFocusOverlay.inspectedPoint) {
      const inspectedScreen = projectMapPointToScreen(
        entityFocusOverlay.inspectedPoint,
        basis
      );
      if (inspectedScreen) {
        drawEntityFocusMarker(
          entityFocusOverlay.inspectedPoint,
          inspectedScreen.x,
          inspectedScreen.y,
          'focus',
          basis
        );
        projectedFocusKeys.add(projectKeyForPoint(entityFocusOverlay.inspectedPoint));
      }
    }

    if (entityFocusOverlay.hoveredPoint) {
      const hoveredKey = projectKeyForPoint(entityFocusOverlay.hoveredPoint);
      if (!projectedFocusKeys.has(hoveredKey)) {
        const hoveredScreen = projectMapPointToScreen(
          entityFocusOverlay.hoveredPoint,
          basis
        );
        if (hoveredScreen) {
          drawEntityFocusMarker(
            entityFocusOverlay.hoveredPoint,
            hoveredScreen.x,
            hoveredScreen.y,
            'focus',
            basis
          );
        }
      }
    }
  }

  function drawEdgeLabel(screenX: number, screenY: number, text: string): void {
    if (!text) {
      return;
    }

    ctx.save();
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    const textWidth = ctx.measureText(text).width;
    const padX = 6;
    const boxHeight = 16;
    const boxWidth = textWidth + padX * 2;
    const boxX = screenX - boxWidth / 2;
    const boxY = screenY - boxHeight / 2;
    ctx.fillStyle = 'rgba(15, 23, 42, 0.86)';
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.35)';
    ctx.lineWidth = 1;
    ctx.fillRect(boxX, boxY, boxWidth, boxHeight);
    ctx.strokeRect(boxX, boxY, boxWidth, boxHeight);
    ctx.fillStyle = '#cbd5e1';
    ctx.fillText(text, screenX, screenY + 0.5);
    ctx.restore();
  }

  function offsetLineLabelPosition(
    from: { x: number; y: number },
    to: { x: number; y: number }
  ): { x: number; y: number } {
    const midX = (from.x + to.x) / 2;
    const midY = (from.y + to.y) / 2;
    const dx = to.x - from.x;
    const dy = to.y - from.y;
    const len = Math.hypot(dx, dy);
    if (!Number.isFinite(len) || len < 1e-4) {
      return { x: midX, y: midY - 10 };
    }

    const nx = -dy / len;
    const ny = dx / len;
    const offsetPx = 12;
    return {
      x: midX + nx * offsetPx,
      y: midY + ny * offsetPx,
    };
  }

  function drawHoverEdgeLabels2D(): void {
    if (hoverEdgeLabels.length === 0) {
      return;
    }

    for (const label of hoverEdgeLabels) {
      const from = worldToScreen2D(label.from.x, label.from.y);
      const to = worldToScreen2D(label.to.x, label.to.y);
      drawEdgeLabel((from.x + to.x) / 2, (from.y + to.y) / 2 - 10, label.text);
    }
  }

  function drawHoverEdgeLabels3D(): void {
    if (hoverEdgeLabels.length === 0) {
      return;
    }

    const basis = getCameraBasis();
    for (const label of hoverEdgeLabels) {
      const from = projectMapPoint3D(label.from, basis);
      const to = projectMapPoint3D(label.to, basis);
      if (!from || !to) {
        continue;
      }
      drawEdgeLabel((from.x + to.x) / 2, (from.y + to.y) / 2 - 10, label.text);
    }
  }

  function drawShapeLineLabels2D(): void {
    if (shapes.length === 0) {
      return;
    }

    for (const shape of shapes) {
      if (shape.type !== 'line') {
        continue;
      }
      const text = String(shape.label ?? '').trim();
      const pts = shape.points ?? [];
      if (!text || pts.length < 2) {
        continue;
      }

      const from = worldToScreen2D(pts[0].x, pts[0].y);
      const to = worldToScreen2D(pts[pts.length - 1].x, pts[pts.length - 1].y);
      const position = offsetLineLabelPosition(from, to);
      drawEdgeLabel(position.x, position.y, text);
    }
  }

  function drawShapeLineLabels3D(): void {
    if (shapes.length === 0) {
      return;
    }

    const basis = getCameraBasis();
    for (const shape of shapes) {
      if (shape.type !== 'line') {
        continue;
      }
      const text = String(shape.label ?? '').trim();
      const pts = shape.points ?? [];
      if (!text || pts.length < 2) {
        continue;
      }

      const from = projectMapPoint3D(pts[0], basis);
      const to = projectMapPoint3D(pts[pts.length - 1], basis);
      if (!from || !to) {
        continue;
      }
      const position = offsetLineLabelPosition(from, to);
      drawEdgeLabel(position.x, position.y, text);
    }
  }

  function getCameraBasis(): {
    position: Vec3;
    forward: Vec3;
    right: Vec3;
    up: Vec3;
  } {
    const offset = getOrbitOffset(cameraDistance, cameraYaw, cameraPitch);
    const position = vecAdd(cameraTarget, offset);

    const forward = vecNormalize(vecSub(cameraTarget, position));
    let right = vecNormalize(vecCross(forward, { x: 0, y: 1, z: 0 }));
    if (vecLength(right) < 1e-5) {
      right = { x: 1, y: 0, z: 0 };
    }
    let up = vecNormalize(vecCross(right, forward));
    if (Math.abs(cameraRoll) > 1e-6) {
      const rollCos = Math.cos(cameraRoll);
      const rollSin = Math.sin(cameraRoll);
      const rolledRight = vecNormalize(
        vecAdd(vecScale(right, rollCos), vecScale(up, rollSin))
      );
      const rolledUp = vecNormalize(
        vecSub(vecScale(up, rollCos), vecScale(right, rollSin))
      );
      right = rolledRight;
      up = rolledUp;
    }
    return { position, forward, right, up };
  }

  type CameraBasis = ReturnType<typeof getCameraBasis>;

  function worldToCamera(point: Vec3, basis: CameraBasis): Vec3 {
    const rel = vecSub(point, basis.position);
    return {
      x: vecDot(rel, basis.right),
      y: vecDot(rel, basis.up),
      z: vecDot(rel, basis.forward),
    };
  }

  function clipLineToDepthRange(
    a: Vec3,
    b: Vec3,
    minDepth: number,
    maxDepth: number
  ): [Vec3, Vec3] | null {
    let t0 = 0;
    let t1 = 1;
    const dz = b.z - a.z;

    if (Math.abs(dz) < 1e-8) {
      if (a.z < minDepth || a.z > maxDepth) {
        return null;
      }
    } else {
      const tAtMin = (minDepth - a.z) / dz;
      const tAtMax = (maxDepth - a.z) / dz;
      const enter = Math.min(tAtMin, tAtMax);
      const exit = Math.max(tAtMin, tAtMax);
      t0 = Math.max(t0, enter);
      t1 = Math.min(t1, exit);
      if (t0 > t1) {
        return null;
      }
    }

    return [vecLerp(a, b, t0), vecLerp(a, b, t1)];
  }

  function currentFarPlane(): number {
    return Math.max(
      DEFAULT_FAR_PLANE_3D,
      Math.abs(cameraDistance) * 64,
      Math.abs(orthoHeight) * 64,
      1_000_000
    );
  }

  function projectCameraPoint(point: Vec3): ProjectedPoint | null {
    const farPlane = currentFarPlane();
    if (point.z < -farPlane || point.z > farPlane) {
      return null;
    }

    const aspect = canvasWidth() / canvasHeight();
    let ndcX = 0;
    let ndcY = 0;

    if (projectionMode === 'perspective') {
      const f = 1 / Math.tan(fovRad / 2);
      ndcX = (point.x * f) / (point.z * aspect);
      ndcY = (point.y * f) / point.z;
    } else {
      const halfHeight = Math.max(Math.abs(orthoHeight), 1e-8);
      const halfWidth = halfHeight * aspect;
      ndcX = point.x / halfWidth;
      ndcY = point.y / halfHeight;
    }

    if (!Number.isFinite(ndcX) || !Number.isFinite(ndcY)) {
      return null;
    }

    // Guard against perspective blowups near the camera that can produce
    // huge translucent "slice" artifacts on the 2D canvas.
    if (projectionMode === 'perspective' && (Math.abs(ndcX) > 24 || Math.abs(ndcY) > 24)) {
      return null;
    }

    return {
      x: (ndcX * 0.5 + 0.5) * canvasWidth(),
      y: (1 - (ndcY * 0.5 + 0.5)) * canvasHeight(),
      depth: point.z,
    };
  }

  function drawLine3D(
    a: Vec3,
    b: Vec3,
    color: string,
    width = 1.2,
    basisOverride?: CameraBasis
  ): void {
    const basis = basisOverride ?? getCameraBasis();
    const cameraA = worldToCamera(a, basis);
    const cameraB = worldToCamera(b, basis);
    const farPlane = currentFarPlane();
    const minDepth = projectionMode === 'perspective' ? NEAR_PLANE : -farPlane;
    const clipped = clipLineToDepthRange(cameraA, cameraB, minDepth, farPlane);
    if (!clipped) {
      return;
    }
    const pa = projectCameraPoint(clipped[0]);
    const pb = projectCameraPoint(clipped[1]);
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

    const basis = getCameraBasis();
    for (let i = 1; i < points.length; i += 1) {
      drawLine3D(points[i - 1], points[i], color, width, basis);
    }
    if (closed && points.length > 2) {
      drawLine3D(points[points.length - 1], points[0], color, width, basis);
    }
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
    ctx.textBaseline = 'top';
    for (let x = Math.floor(startX / step) * step; x <= endX; x += step) {
      const screenX = x * scale2D + offsetX;
      ctx.beginPath();
      ctx.moveTo(screenX, 0);
      ctx.lineTo(screenX, canvasHeight());
      ctx.stroke();
      ctx.save();
      ctx.scale(1, -1);
      ctx.fillText(x.toFixed(0), screenX, -canvasHeight() + 2);
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

  function drawAxisGizmo2D(): void {
    const axisOrigin = { x: 54, y: 54 };
    const axisSize = 24;
    const cosRoll = Math.cos(cameraRoll);
    const sinRoll = Math.sin(cameraRoll);

    // In 2D map mode, world X/Y are the visible plane axes.
    const xEnd = {
      x: axisOrigin.x + cosRoll * axisSize,
      y: axisOrigin.y - sinRoll * axisSize,
    };
    const yEnd = {
      x: axisOrigin.x - sinRoll * axisSize,
      y: axisOrigin.y - cosRoll * axisSize,
    };

    ctx.lineWidth = 2;
    ctx.strokeStyle = 'rgba(239, 68, 68, 0.9)';
    ctx.beginPath();
    ctx.moveTo(axisOrigin.x, axisOrigin.y);
    ctx.lineTo(xEnd.x, xEnd.y);
    ctx.stroke();

    ctx.strokeStyle = 'rgba(59, 130, 246, 0.9)';
    ctx.beginPath();
    ctx.moveTo(axisOrigin.x, axisOrigin.y);
    ctx.lineTo(yEnd.x, yEnd.y);
    ctx.stroke();

    ctx.fillStyle = 'rgba(226, 232, 240, 0.9)';
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText('X', xEnd.x + 4, xEnd.y);
    ctx.fillText('Y', yEnd.x + 4, yEnd.y);
  }

  function drawAxes3D(): void {
    const basis = getCameraBasis();
    const width = canvasWidth();
    const height = canvasHeight();
    const aspect = width / height;
    const maxAxisSpanFactor = Math.max(1, aspect);
    const targetPlaneSpanRef =
      projectionMode === 'perspective'
        ? Math.max(
            2,
            2 * Math.abs(cameraDistance) * Math.tan(fovRad / 2) * maxAxisSpanFactor
          )
        : Math.max(2, 2 * Math.abs(orthoHeight) * maxAxisSpanFactor);
    const unitsPerPixel =
      targetPlaneSpanRef / Math.max(1, Math.max(width, height));
    const desiredGridPixelSpacing = 72;
    // Keep grid detail tied to camera scale (zoom/FOV), not camera orientation,
    // so rotating does not cause line density popping.
    const step = niceStep(
      Math.max(0.25, unitsPerPixel * desiredGridPixelSpacing)
    );
    const lineCountPerSide = Math.max(
      GRID_LINE_COUNT_PER_SIDE,
      Math.ceil((targetPlaneSpanRef * 0.9) / Math.max(step, 1e-4))
    );
    const anchorX = Math.floor(cameraTarget.x / step) * step;
    const anchorZ = Math.floor(cameraTarget.z / step) * step;

    ctx.lineWidth = 1;
    for (let xi = -lineCountPerSide; xi <= lineCountPerSide; xi += 1) {
      const x = anchorX + xi * step;
      for (let zi = -lineCountPerSide; zi < lineCountPerSide; zi += 1) {
        const z0 = anchorZ + zi * step;
        const z1 = anchorZ + (zi + 1) * step;
        drawLine3D(
          { x, y: 0, z: z0 },
          { x, y: 0, z: z1 },
          'rgba(148, 163, 184, 0.25)',
          1,
          basis
        );
      }
    }

    for (let zi = -lineCountPerSide; zi <= lineCountPerSide; zi += 1) {
      const z = anchorZ + zi * step;
      for (let xi = -lineCountPerSide; xi < lineCountPerSide; xi += 1) {
        const x0 = anchorX + xi * step;
        const x1 = anchorX + (xi + 1) * step;
        drawLine3D(
          { x: x0, y: 0, z },
          { x: x1, y: 0, z },
          'rgba(148, 163, 184, 0.25)',
          1,
          basis
        );
      }
    }

    const axisSegmentCount = Math.max(lineCountPerSide, GRID_LINE_COUNT_PER_SIDE * 2);
    const axisAnchorX = Math.floor(cameraTarget.x / step) * step;
    const axisAnchorY = Math.floor(cameraTarget.y / step) * step;
    const axisAnchorZ = Math.floor(cameraTarget.z / step) * step;
    for (let i = -axisSegmentCount; i < axisSegmentCount; i += 1) {
      const x0 = axisAnchorX + i * step;
      const x1 = axisAnchorX + (i + 1) * step;
      drawLine3D(
        { x: x0, y: 0, z: 0 },
        { x: x1, y: 0, z: 0 },
        'rgba(239, 68, 68, 0.95)',
        2.2,
        basis
      );
    }
    for (let i = -axisSegmentCount; i < axisSegmentCount; i += 1) {
      const y0 = axisAnchorY + i * step;
      const y1 = axisAnchorY + (i + 1) * step;
      drawLine3D(
        { x: 0, y: y0, z: 0 },
        { x: 0, y: y1, z: 0 },
        'rgba(34, 197, 94, 0.95)',
        2.2,
        basis
      );
    }
    for (let i = -axisSegmentCount; i < axisSegmentCount; i += 1) {
      const z0 = axisAnchorZ + i * step;
      const z1 = axisAnchorZ + (i + 1) * step;
      drawLine3D(
        { x: 0, y: 0, z: z0 },
        { x: 0, y: 0, z: z1 },
        'rgba(59, 130, 246, 0.95)',
        2.2,
        basis
      );
    }

    // Camera target indicator
    const projectedTarget = projectCameraPoint(worldToCamera(cameraTarget, basis));
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

    const facing = axisForward.y > 0 ? 'Up' : 'Down';
    ctx.fillStyle = 'rgba(226, 232, 240, 0.9)';
    ctx.font = '11px sans-serif';
    ctx.fillText(facing, axisOrigin.x - 28, axisOrigin.y - 24);
    ctx.fillText('X', xEnd.x + 4, xEnd.y - 2);
    // In map semantics, source Z is vertical; world Y represents that vertical axis.
    ctx.fillText('Z', yEnd.x + 4, yEnd.y - 2);
    ctx.fillText('Y', zEnd.x + 4, zEnd.y - 2);
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
          const radius = shape.radius ?? 10;
          ctx.beginPath();
          ctx.arc(0, 0, radius, 0, Math.PI * 2);
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
      const centerX = canvasWidth() / 2;
      const centerY = canvasHeight() / 2;
      ctx.translate(centerX, centerY);
      ctx.rotate(cameraRoll);
      ctx.translate(-centerX, -centerY);
      drawAxes2D();
      drawShapes2D();
      ctx.restore();
      drawAxisGizmo2D();
      drawHoverEdgeLabels2D();
      drawShapeLineLabels2D();
      drawEntityFocusOverlay();
      if (lastPointerScreen) {
        onPointerWorldPosition?.(
          screenToWorldMap(lastPointerScreen.x, lastPointerScreen.y),
          lastPointerScreen
        );
      }
      return;
    }

    // 3D
    drawAxes3D();
    drawShapes3D();
    drawHoverEdgeLabels3D();
    drawShapeLineLabels3D();
    drawEntityFocusOverlay();

    if (lastPointerScreen) {
      onPointerWorldPosition?.(
        screenToWorldMap(lastPointerScreen.x, lastPointerScreen.y),
        lastPointerScreen
      );
    }
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
      cameraRoll = 0;
      draw();
      return;
    }

    const centerX = (bounds.minX + bounds.maxX) / 2;
    const centerZ = (bounds.minZ + bounds.maxZ) / 2;
    const sizeX = Math.max(10, bounds.maxX - bounds.minX);
    const sizeZ = Math.max(10, bounds.maxZ - bounds.minZ);
    const extent = Math.max(sizeX, sizeZ);

    cameraTarget = { x: centerX, y: 0, z: centerZ };
    cameraDistance = Math.max(extent * 1.8, 45);
    orthoHeight = Math.max(extent * 0.85, 12);
    cameraYaw = Math.PI / 4;
    cameraPitch = 0.75;
    cameraRoll = 0;

    scale2D = clamp(Math.min(canvasWidth(), canvasHeight()) / (extent * 1.4), MIN_SCALE_2D, MAX_SCALE_2D);
    offsetX = canvasWidth() / 2 - centerX * scale2D;
    offsetY = canvasHeight() / 2 - centerZ * scale2D;

    draw();
  }

  function setViewPreset(preset: MapViewPreset): void {
    viewMode = '3d';
    cameraRoll = 0;
    if (preset === 'top') {
      cameraYaw = 0;
      cameraPitch = MAX_PITCH;
    } else if (preset === 'front') {
      cameraYaw = 0;
      cameraPitch = 0;
    } else if (preset === 'right') {
      cameraYaw = Math.PI / 2;
      cameraPitch = 0;
    } else {
      cameraYaw = Math.PI / 4;
      cameraPitch = 0.75;
    }
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

  function hasFlyInput(): boolean {
    return (
      flyKeys.has('w') ||
      flyKeys.has('a') ||
      flyKeys.has('s') ||
      flyKeys.has('d')
    );
  }

  function stopFlyLoop(): void {
    if (flyRafId != null) {
      cancelAnimationFrame(flyRafId);
      flyRafId = null;
    }
    lastFlyFrameMs = null;
  }

  function tickFly(frameMs: number): void {
    if (!hasFlyInput()) {
      stopFlyLoop();
      return;
    }

    if (lastFlyFrameMs == null) {
      lastFlyFrameMs = frameMs;
    }

    const dtSec = clamp((frameMs - lastFlyFrameMs) / 1000, 0, 0.05);
    lastFlyFrameMs = frameMs;

    let didChange = false;

    if (viewMode === '2d') {
      const move2D = {
        x: (flyKeys.has('a') ? 1 : 0) + (flyKeys.has('d') ? -1 : 0),
        y: (flyKeys.has('w') ? 1 : 0) + (flyKeys.has('s') ? -1 : 0),
      };
      const move2DLen = Math.hypot(move2D.x, move2D.y);
      if (move2DLen > 1e-6) {
        const worldViewExtent = Math.max(
          20,
          canvasWidth() / Math.max(scale2D, 1e-4),
          canvasHeight() / Math.max(scale2D, 1e-4)
        );
        let panSpeedWorld = worldViewExtent * 0.55 * interactionSensitivity;
        if (flyKeys.has('shift')) {
          panSpeedWorld *= CAMERA_FLY_SHIFT_MULTIPLIER;
        }
        const moveScale = (panSpeedWorld * dtSec) / move2DLen;
        const cameraVecX = move2D.x;
        const cameraVecY = -move2D.y;
        const rollCos = Math.cos(cameraRoll);
        const rollSin = Math.sin(cameraRoll);
        const worldVecX = cameraVecX * rollCos + cameraVecY * rollSin;
        const worldVecY = -cameraVecX * rollSin + cameraVecY * rollCos;
        offsetX += worldVecX * moveScale * scale2D;
        offsetY += worldVecY * moveScale * scale2D;
        didChange = true;
      }

    } else {
      const basis = getCameraBasis();
      let planarForward = {
        x: basis.forward.x,
        y: 0,
        z: basis.forward.z,
      };
      let planarForwardLen = vecLength(planarForward);
      if (planarForwardLen < 1e-6) {
        planarForward = { x: 0, y: 0, z: -1 };
        planarForwardLen = 1;
      }
      planarForward = vecScale(planarForward, 1 / planarForwardLen);
      const planarRight = vecNormalize(
        vecCross(planarForward, { x: 0, y: 1, z: 0 })
      );

      let move = { x: 0, y: 0, z: 0 };

      // Camera-relative planar movement: S is backward in 3D.
      if (flyKeys.has('w')) move = vecAdd(move, planarForward);
      if (flyKeys.has('s')) move = vecSub(move, planarForward);
      if (flyKeys.has('d')) move = vecAdd(move, planarRight);
      if (flyKeys.has('a')) move = vecSub(move, planarRight);

      const moveLen = vecLength(move);
      if (moveLen > 1e-6) {
        const normalizedMove = vecScale(move, 1 / moveLen);
        const speedRef =
          projectionMode === 'perspective' ? cameraDistance * 0.55 : orthoHeight * 0.55;
        let speed = Math.max(CAMERA_FLY_BASE_SPEED, speedRef) * interactionSensitivity;
        if (flyKeys.has('shift')) {
          speed *= CAMERA_FLY_SHIFT_MULTIPLIER;
        }
        cameraTarget = vecAdd(cameraTarget, vecScale(normalizedMove, speed * dtSec));
        didChange = true;
      }

    }

    if (didChange) {
      draw();
    }

    flyRafId = requestAnimationFrame(tickFly);
  }

  function startFlyLoop(): void {
    if (flyRafId == null && hasFlyInput()) {
      flyRafId = requestAnimationFrame(tickFly);
    }
  }

  function onPointerDown(event: PointerEvent): void {
    canvas.focus();
    activePointerId = event.pointerId;
    lastPointerX = event.clientX;
    lastPointerY = event.clientY;

    if (viewMode === '2d') {
      if (event.button === 0) {
        pointerDragMode = 'pan2d';
        canvas.setPointerCapture(event.pointerId);
      }
      return;
    }

    const orbitRequested = event.button === 2;
    const panRequested = event.button === 0;

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
    const rect = canvas.getBoundingClientRect();
    const pointerScreen = {
      x: event.clientX - rect.left,
      y: event.clientY - rect.top,
    };
    lastPointerScreen = pointerScreen;
    onPointerWorldPosition?.(
      screenToWorldMap(pointerScreen.x, pointerScreen.y),
      pointerScreen
    );

    if (activePointerId !== event.pointerId || pointerDragMode === 'none') {
      return;
    }

    const dx = event.clientX - lastPointerX;
    const dy = event.clientY - lastPointerY;
    lastPointerX = event.clientX;
    lastPointerY = event.clientY;

    if (pointerDragMode === 'pan2d') {
      const deltaViewX = dx * interactionSensitivity;
      const deltaViewY = -dy * interactionSensitivity;
      const rollCos = Math.cos(cameraRoll);
      const rollSin = Math.sin(cameraRoll);
      offsetX += deltaViewX * rollCos + deltaViewY * rollSin;
      offsetY += -deltaViewX * rollSin + deltaViewY * rollCos;
      draw();
      return;
    }

    if (pointerDragMode === 'orbit3d') {
      const orbitSpeed = CAMERA_ORBIT_SPEED * interactionSensitivity;
      const pivotPosition = getCameraBasis().position;
      cameraYaw -= dx * orbitSpeed;
      cameraPitch = clamp(cameraPitch - dy * orbitSpeed, MIN_PITCH, MAX_PITCH);
      const offset = getOrbitOffset(cameraDistance, cameraYaw, cameraPitch);
      cameraTarget = {
        x: pivotPosition.x - offset.x,
        y: pivotPosition.y - offset.y,
        z: pivotPosition.z - offset.z,
      };
      draw();
      return;
    }

    if (pointerDragMode === 'pan3d') {
      const basis = getCameraBasis();
      const panMagnitude =
        projectionMode === 'perspective'
          ? cameraDistance * CAMERA_PAN_SCALE
          : Math.max(Math.abs(orthoHeight), 1e-8) * CAMERA_PAN_SCALE;
      const panScale = panMagnitude * interactionSensitivity;

      const panRight = vecScale(basis.right, -dx * panScale);
      const panUp = vecScale(basis.up, dy * panScale);
      cameraTarget = vecAdd(cameraTarget, vecAdd(panRight, panUp));
      draw();
    }
  }

  function onPointerLeave(): void {
    lastPointerScreen = null;
    onPointerWorldPosition?.(null, null);
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
    if (interactionSensitivity <= 0) {
      return;
    }
    const zoomOutFactor = 1.1;
    const zoomInFactor = 1 / zoomOutFactor;
    const zoomFactor = event.deltaY > 0 ? zoomOutFactor : zoomInFactor;
    const sensitivityZoomFactor = Math.pow(zoomFactor, interactionSensitivity);

    if (viewMode === '2d') {
      const centerX = canvasWidth() / 2;
      const centerY = canvasHeight() / 2;
      const mouseViewX = event.offsetX;
      const mouseViewY = canvasHeight() - event.offsetY;
      const rollCos = Math.cos(cameraRoll);
      const rollSin = Math.sin(cameraRoll);
      const dxView = mouseViewX - centerX;
      const dyView = mouseViewY - centerY;
      const mouseX = centerX + dxView * rollCos + dyView * rollSin;
      const mouseY = centerY - dxView * rollSin + dyView * rollCos;
      const nextScale = clamp(
        scale2D / sensitivityZoomFactor,
        MIN_SCALE_2D,
        MAX_SCALE_2D
      );

      offsetX = mouseX - ((mouseX - offsetX) * nextScale) / scale2D;
      offsetY = mouseY - ((mouseY - offsetY) * nextScale) / scale2D;
      scale2D = nextScale;
      draw();
      return;
    }

    if (projectionMode === 'perspective') {
      cameraDistance *= sensitivityZoomFactor;
    } else {
      orthoHeight *= sensitivityZoomFactor;
    }
    draw();
  }

  function onContextMenu(event: MouseEvent): void {
    event.preventDefault();
  }

  const flyKeyNames = new Set(['w', 'a', 's', 'd', 'shift']);

  function shouldIgnoreKeyboardInputEvent(event: KeyboardEvent): boolean {
    if (event.metaKey || event.ctrlKey || event.altKey) {
      return true;
    }
    const target = event.target;
    if (!(target instanceof HTMLElement)) {
      return false;
    }
    if (target.isContentEditable) {
      return true;
    }
    const tag = target.tagName;
    return tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';
  }

  function onKeyDown(event: KeyboardEvent): void {
    if (shouldIgnoreKeyboardInputEvent(event)) {
      return;
    }

    const key = event.key.toLowerCase();
    if (key === 'f') {
      resetCamera();
      event.preventDefault();
      return;
    }

    if (flyKeyNames.has(key)) {
      flyKeys.add(key);
      startFlyLoop();
      event.preventDefault();
    }
  }

  function onKeyUp(event: KeyboardEvent): void {
    const key = event.key.toLowerCase();
    if (flyKeyNames.has(key)) {
      flyKeys.delete(key);
      if (!hasFlyInput()) {
        stopFlyLoop();
      }
    }
  }

  function onBlur(): void {
    flyKeys.clear();
    stopFlyLoop();
    lastPointerScreen = null;
    onPointerWorldPosition?.(null, null);
  }

  window.addEventListener('resize', resizeCanvas);
  window.addEventListener('keydown', onKeyDown);
  window.addEventListener('keyup', onKeyUp);
  window.addEventListener('blur', onBlur);
  canvas.addEventListener('pointerdown', onPointerDown);
  canvas.addEventListener('pointermove', onPointerMove);
  canvas.addEventListener('pointerleave', onPointerLeave);
  canvas.addEventListener('pointerup', endPointerDrag);
  canvas.addEventListener('pointercancel', endPointerDrag);
  canvas.addEventListener('wheel', onWheel, { passive: false });
  canvas.addEventListener('contextmenu', onContextMenu);

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
    getViewState2D() {
      return {
        scale: scale2D,
        offsetX,
        offsetY,
      };
    },
    setInteractionSensitivity(nextSensitivity: number) {
      interactionSensitivity = normalizeInteractionSensitivity(
        nextSensitivity,
        MIN_INTERACTION_SENSITIVITY
      );
    },
    setViewMode(nextViewMode: MapViewMode) {
      viewMode = nextViewMode;
      draw();
    },
    setViewPreset,
    setProjectionMode(nextProjectionMode: MapProjectionMode) {
      projectionMode = nextProjectionMode;
      draw();
    },
    setHoverEdgeLabels(nextLabels: EdgeLabelOverlay[]) {
      hoverEdgeLabels = nextLabels;
      draw();
    },
    setEntityFocusOverlay(nextOverlay: EntityFocusOverlay) {
      const selectedPoints = Array.isArray(nextOverlay.selectedPoints)
        ? nextOverlay.selectedPoints.filter(
            (point) =>
              Number.isFinite(point.x) &&
              Number.isFinite(point.y)
          )
        : [];
      const hoveredPoint =
        nextOverlay.hoveredPoint &&
        Number.isFinite(nextOverlay.hoveredPoint.x) &&
        Number.isFinite(nextOverlay.hoveredPoint.y)
          ? nextOverlay.hoveredPoint
          : null;
      const inspectedPoint =
        nextOverlay.inspectedPoint &&
        Number.isFinite(nextOverlay.inspectedPoint.x) &&
        Number.isFinite(nextOverlay.inspectedPoint.y)
          ? nextOverlay.inspectedPoint
          : null;
      entityFocusOverlay = {
        enabled: Boolean(nextOverlay.enabled),
        selectedPoints,
        inspectedPoint,
        hoveredPoint,
      };
      draw();
    },
    projectMapPoint(point: { x: number; y: number; z?: number }) {
      const x = Number(point.x);
      const y = Number(point.y);
      if (!Number.isFinite(x) || !Number.isFinite(y)) {
        return null;
      }
      return projectMapPointToScreen({ x, y });
    },
    resetCamera,
    destroy() {
      window.removeEventListener('resize', resizeCanvas);
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', onBlur);
      canvas.removeEventListener('pointerdown', onPointerDown);
      canvas.removeEventListener('pointermove', onPointerMove);
      canvas.removeEventListener('pointerleave', onPointerLeave);
      canvas.removeEventListener('pointerup', endPointerDrag);
      canvas.removeEventListener('pointercancel', endPointerDrag);
      canvas.removeEventListener('wheel', onWheel);
      canvas.removeEventListener('contextmenu', onContextMenu);
      stopFlyLoop();
      if (canvas.parentElement === container) {
        container.removeChild(canvas);
      }
    },
  };
}

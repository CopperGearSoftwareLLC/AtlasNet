import {
  AfterViewInit,
  ChangeDetectionStrategy,
  ChangeDetectorRef,
  Component,
  ElementRef,
  HostListener,
  NgZone,
  OnDestroy,
  ViewChild,
  ViewEncapsulation
} from '@angular/core';
import { Subject } from 'rxjs';
import { takeUntil } from 'rxjs/operators';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { CartographMapResponse, CartographTile, CartographTopology, MapService } from './map.service';

@Component({
  selector: 'map-dashboard',
  templateUrl: './map.template.html',
  styleUrls: ['./map.style.scss'],
  encapsulation: ViewEncapsulation.None,
  changeDetection: ChangeDetectionStrategy.OnPush
})
export class MapComponent implements AfterViewInit, OnDestroy {
  private static readonly CAMERA_ROTATE_SPEED = 0.3;
  private static readonly VERTICAL_EXAGGERATION = 1.65;
  private static readonly TERRAIN_CHUNK_SIZE = 32;

  @ViewChild('canvasHost', { static: false }) public canvasHost?: ElementRef<HTMLDivElement>;

  public isLoading: boolean = true;
  public errorMessage: string | null = null;
  public mapResponse: CartographMapResponse | null = null;
  public mapStats: MapStats | null = null;
  public lightBrightness: number = 50;
  public lightRotation: number = 35;
  public terrainColorEnabled: boolean = false;

  private renderer?: any;
  private scene?: any;
  private camera?: any;
  private controls?: any;
  private ambientLight?: any;
  private fillLight?: any;
  private keyLight?: any;
  private terrainChunks: TerrainChunk[] = [];
  private activeTopology?: NormalizedTopology;
  private keyLightRadius: number = 0;
  private keyLightHeight: number = 0;
  private frameId?: number;
  private disposables: any[] = [];
  private readonly cullingFrustum = new THREE.Frustum();
  private readonly cullingMatrix = new THREE.Matrix4();
  private readonly cullingSphere = new THREE.Sphere();
  private readonly destroy$ = new Subject<void>();

  constructor(
    private readonly ngZone: NgZone,
    private readonly mapService: MapService,
    private readonly changeDetectorRef: ChangeDetectorRef
  ) {}

  @HostListener('window:resize')
  public onWindowResize(): void {
    this.resizeRenderer();
  }

  public ngAfterViewInit(): void {
    this.mapService.loadMap()
      .pipe(takeUntil(this.destroy$))
      .subscribe({
        next: (response: CartographMapResponse) => {
          this.mapResponse = response;
          this.isLoading = false;

          if (!response.topologyAvailable || !response.topology) {
            this.errorMessage = response.message || 'Topology JSON not found in cartograph-map';
            this.changeDetectorRef.markForCheck();
            return;
          }

          this.errorMessage = null;
          this.initializeScene(response.topology);
          this.changeDetectorRef.markForCheck();
        },
        error: () => {
          this.isLoading = false;
          this.errorMessage = 'Map data could not be loaded from the backend or the local cartograph-map assets.';
          this.changeDetectorRef.markForCheck();
        }
      });
  }

  public ngOnDestroy(): void {
    this.destroy$.next();
    this.destroy$.complete();
    this.disposeScene();
  }

  public onLightBrightnessChange(value: string): void {
    const nextValue = Number(value);
    if (!Number.isFinite(nextValue)) {
      return;
    }

    this.lightBrightness = nextValue;
    this.applyLightingLevels();
    this.scheduleRender();
  }

  public onLightRotationChange(value: string): void {
    const nextValue = Number(value);
    if (!Number.isFinite(nextValue)) {
      return;
    }

    this.lightRotation = nextValue;
    this.updateKeyLightPosition();
    this.invalidateShadows();
    this.scheduleRender();
  }

  public onTerrainColorToggle(checked: boolean): void {
    this.terrainColorEnabled = checked;
    this.syncTerrainAppearance();
  }

  private async initializeScene(topology: CartographTopology): Promise<void> {
    const host = this.canvasHost?.nativeElement;
    if (!host) {
      return;
    }

    this.disposeScene();

    const normalized = this.normalizeTopology(topology);
    if (!normalized) {
      this.errorMessage = 'Topology JSON is present, but it does not match the expected height-map shape.';
      this.changeDetectorRef.markForCheck();
      return;
    }

    const width = host.clientWidth || 960;
    const height = host.clientHeight || 560;
    const cameraDistance = Math.max(normalized.maxSpan * 1.05, normalized.maxHeight * 10 + 18);
    const cameraElevation = Math.max(normalized.maxHeight * 4.2, normalized.maxSpan * 0.28);
    const farPlane = cameraDistance + normalized.maxSpan * 4;

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color('#0a1118');
    this.scene.fog = new THREE.Fog('#0a1118', cameraDistance * 0.55, farPlane * 0.92);

    this.camera = new THREE.PerspectiveCamera(34, width / height, 0.1, farPlane);
    this.camera.position.set(0, cameraElevation, cameraDistance);
    this.camera.lookAt(0, normalized.maxHeight * 0.72, normalized.maxSpan * -0.05);

    this.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'low-power' });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.25));
    this.renderer.setSize(width, height);
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.shadowMap.autoUpdate = false;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 0.9;
    if (this.renderer.outputColorSpace !== undefined && THREE.SRGBColorSpace) {
      this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    }
    host.appendChild(this.renderer.domElement);

    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = false;
    this.controls.rotateSpeed = MapComponent.CAMERA_ROTATE_SPEED;
    this.controls.screenSpacePanning = true;
    this.controls.minDistance = Math.max(normalized.maxSpan * 0.08, 12);
    this.controls.maxDistance = farPlane * 0.45;
    this.controls.maxPolarAngle = Math.PI / 2 - 0.06;
    this.controls.target.set(0, normalized.maxHeight * 0.55, 0);
    this.controls.addEventListener('change', () => {
      this.updateChunkVisibility();
      this.scheduleRender();
    });
    this.controls.update();

    this.ambientLight = new THREE.AmbientLight('#d7e0ea', 0.52);
    this.keyLight = new THREE.DirectionalLight('#f7f1dc', this.lightBrightness);
    this.keyLightRadius = normalized.maxSpan * 0.5;
    this.keyLightHeight = normalized.maxHeight * 4.2 + 42;
    this.keyLight.castShadow = true;
    this.keyLight.shadow.mapSize.width = 1536;
    this.keyLight.shadow.mapSize.height = 1536;
    this.keyLight.shadow.bias = -0.00015;
    this.keyLight.shadow.normalBias = 0.035;
    this.keyLight.shadow.camera.near = 1;
    this.keyLight.shadow.camera.far = farPlane;
    this.keyLight.shadow.camera.left = -normalized.maxSpan * 0.8;
    this.keyLight.shadow.camera.right = normalized.maxSpan * 0.8;
    this.keyLight.shadow.camera.top = normalized.maxSpan * 0.8;
    this.keyLight.shadow.camera.bottom = -normalized.maxSpan * 0.8;
    this.updateKeyLightPosition();
    this.fillLight = new THREE.DirectionalLight('#7f93a8', 0.22);
    this.fillLight.position.set(-normalized.maxSpan * 0.7, normalized.maxHeight * 1.8 + 18, -normalized.maxSpan * 0.6);
    this.applyLightingLevels();
    this.scene.add(this.ambientLight, this.keyLight, this.fillLight);

    this.addGround(normalized.maxSpan);
    this.activeTopology = normalized;
    this.addVoxelTerrain(normalized);
    this.updateChunkVisibility();
    this.syncTerrainAppearance();
    this.mapStats = {
      format: topology.format || 'unknown',
      tileCount: normalized.voxelCount,
      width: normalized.width,
      depth: normalized.depth,
      maxHeight: normalized.maxHeightValue
    };

    this.invalidateShadows();
    this.scheduleRender();
  }

  private addGround(maxSpan: number): void {
    const planeGeometry = new THREE.PlaneGeometry(maxSpan * 2.8, maxSpan * 2.8, 1, 1);
    const planeMaterial = new THREE.MeshPhongMaterial({
      color: '#0d0d0d',
      shininess: 3
    });
    const plane = new THREE.Mesh(planeGeometry, planeMaterial);
    plane.rotation.x = -Math.PI / 2;
    plane.position.y = -0.02;
    plane.receiveShadow = true;

    const grid = new THREE.GridHelper(maxSpan * 2.6, 18, '#303030', '#161616');
    grid.position.y = 0.01;

    this.trackDisposable(planeGeometry, planeMaterial);
    this.scene.add(plane, grid);
  }

  private addVoxelTerrain(topology: NormalizedTopology): void {
    const voxelCount = topology.voxelCount;

    if (!voxelCount) {
      return;
    }

    const material = new THREE.MeshStandardMaterial({
      color: '#ffffff',
      vertexColors: true,
      roughness: 0.96,
      metalness: 0,
      flatShading: true
    });
    const chunkSize = MapComponent.TERRAIN_CHUNK_SIZE;

    for (let startZ = 0; startZ < topology.depth; startZ += chunkSize) {
      for (let startX = 0; startX < topology.width; startX += chunkSize) {
        const endX = Math.min(startX + chunkSize, topology.width);
        const endZ = Math.min(startZ + chunkSize, topology.depth);
        const chunkGeometry = this.buildTerrainChunkGeometry(topology, startX, endX, startZ, endZ);
        if (!chunkGeometry) {
          continue;
        }

        const mesh = new THREE.Mesh(chunkGeometry, material);
        mesh.castShadow = true;
        mesh.receiveShadow = true;

        this.terrainChunks.push({
          mesh,
          startX,
          endX,
          startZ,
          endZ,
          center: chunkGeometry.boundingSphere ? chunkGeometry.boundingSphere.center.clone() : new THREE.Vector3(),
          radius: chunkGeometry.boundingSphere ? chunkGeometry.boundingSphere.radius : topology.cellSize
        });
        this.trackDisposable(chunkGeometry);
        this.scene.add(mesh);
      }
    }

    this.trackDisposable(material);
  }

  private syncTerrainAppearance(): void {
    this.applyTerrainColors();

    if (this.terrainChunks.length && this.terrainChunks[0].mesh?.material) {
      this.terrainChunks[0].mesh.material.needsUpdate = true;
    }

    this.scheduleRender();
  }

  private applyTerrainColors(): void {
    if (!this.terrainChunks.length || !this.activeTopology) {
      return;
    }

    this.terrainChunks.forEach((chunk: TerrainChunk) => {
      const colors = this.buildTerrainChunkColors(this.activeTopology!, chunk.startX, chunk.endX, chunk.startZ, chunk.endZ);
      chunk.mesh.geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
      chunk.mesh.geometry.attributes.color.needsUpdate = true;
    });
  }

  private buildTerrainChunkGeometry(
    topology: NormalizedTopology,
    startX: number,
    endX: number,
    startZ: number,
    endZ: number
  ): any | null {
    const positions: number[] = [];
    const normals: number[] = [];
    const colors = this.buildTerrainChunkColors(topology, startX, endX, startZ, endZ);
    const halfSize = topology.cellSize * 0.5;

    for (let z = startZ; z < endZ; z += 1) {
      for (let x = startX; x < endX; x += 1) {
        const rawHeight = topology.heights[z][x];
        if (rawHeight <= 0) {
          continue;
        }

        const topY = this.resolveScaledHeight(topology, rawHeight);
        const centerX = (x - topology.halfWidth) * topology.cellSize;
        const centerZ = (z - topology.halfDepth) * topology.cellSize;

        this.pushTopPlate(positions, normals, centerX, centerZ, topY, halfSize);
        this.pushStepWalls(positions, normals, topology, x, z, centerX, centerZ, topY, halfSize);
      }
    }

    if (!positions.length) {
      return null;
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
    geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();
    return geometry;
  }

  private buildTerrainChunkColors(
    topology: NormalizedTopology,
    startX: number,
    endX: number,
    startZ: number,
    endZ: number
  ): number[] {
    const colors: number[] = [];

    for (let z = startZ; z < endZ; z += 1) {
      for (let x = startX; x < endX; x += 1) {
        const rawHeight = topology.heights[z][x];
        if (rawHeight <= 0) {
          continue;
        }

        const topY = this.resolveScaledHeight(topology, rawHeight);
        const baseColor = this.resolveTerrainColor(topology, x, z, rawHeight);
        this.pushFaceColor(colors, baseColor, 1);
        this.pushStepWallColors(colors, topology, x, z, topY, baseColor);
      }
    }

    return colors;
  }

  private pushTopPlate(
    positions: number[],
    normals: number[],
    centerX: number,
    centerZ: number,
    topY: number,
    halfSize: number
  ): void {
    positions.push(
      centerX - halfSize, topY, centerZ - halfSize,
      centerX + halfSize, topY, centerZ + halfSize,
      centerX + halfSize, topY, centerZ - halfSize,
      centerX - halfSize, topY, centerZ - halfSize,
      centerX - halfSize, topY, centerZ + halfSize,
      centerX + halfSize, topY, centerZ + halfSize
    );
    this.pushFaceNormal(normals, 0, 1, 0);
  }

  private pushStepWalls(
    positions: number[],
    normals: number[],
    topology: NormalizedTopology,
    x: number,
    z: number,
    centerX: number,
    centerZ: number,
    topY: number,
    halfSize: number
  ): void {
    const westHeight = this.resolveNeighborHeight(topology, x - 1, z);
    if (topY > westHeight) {
      this.pushVerticalWall(
        positions,
        normals,
        centerX - halfSize,
        centerZ + halfSize,
        centerZ - halfSize,
        westHeight,
        topY,
        -1,
        0,
        0
      );
    }

    const eastHeight = this.resolveNeighborHeight(topology, x + 1, z);
    if (topY > eastHeight) {
      this.pushVerticalWall(
        positions,
        normals,
        centerX + halfSize,
        centerZ - halfSize,
        centerZ + halfSize,
        eastHeight,
        topY,
        1,
        0,
        0
      );
    }

    const northHeight = this.resolveNeighborHeight(topology, x, z - 1);
    if (topY > northHeight) {
      this.pushDepthWall(
        positions,
        normals,
        centerZ - halfSize,
        centerX - halfSize,
        centerX + halfSize,
        northHeight,
        topY,
        0,
        0,
        -1
      );
    }

    const southHeight = this.resolveNeighborHeight(topology, x, z + 1);
    if (topY > southHeight) {
      this.pushDepthWall(
        positions,
        normals,
        centerZ + halfSize,
        centerX + halfSize,
        centerX - halfSize,
        southHeight,
        topY,
        0,
        0,
        1
      );
    }
  }

  private pushStepWallColors(
    colors: number[],
    topology: NormalizedTopology,
    x: number,
    z: number,
    topY: number,
    baseColor: any
  ): void {
    if (topY > this.resolveNeighborHeight(topology, x - 1, z)) {
      this.pushFaceColor(colors, baseColor, 0.72);
    }

    if (topY > this.resolveNeighborHeight(topology, x + 1, z)) {
      this.pushFaceColor(colors, baseColor, 0.78);
    }

    if (topY > this.resolveNeighborHeight(topology, x, z - 1)) {
      this.pushFaceColor(colors, baseColor, 0.68);
    }

    if (topY > this.resolveNeighborHeight(topology, x, z + 1)) {
      this.pushFaceColor(colors, baseColor, 0.74);
    }
  }

  private pushVerticalWall(
    positions: number[],
    normals: number[],
    x: number,
    nearZ: number,
    farZ: number,
    bottomY: number,
    topY: number,
    normalX: number,
    normalY: number,
    normalZ: number
  ): void {
    positions.push(
      x, bottomY, nearZ,
      x, topY, nearZ,
      x, topY, farZ,
      x, bottomY, nearZ,
      x, topY, farZ,
      x, bottomY, farZ
    );
    this.pushFaceNormal(normals, normalX, normalY, normalZ);
  }

  private pushDepthWall(
    positions: number[],
    normals: number[],
    z: number,
    nearX: number,
    farX: number,
    bottomY: number,
    topY: number,
    normalX: number,
    normalY: number,
    normalZ: number
  ): void {
    positions.push(
      nearX, bottomY, z,
      nearX, topY, z,
      farX, topY, z,
      nearX, bottomY, z,
      farX, topY, z,
      farX, bottomY, z
    );
    this.pushFaceNormal(normals, normalX, normalY, normalZ);
  }

  private pushFaceNormal(normals: number[], x: number, y: number, z: number): void {
    for (let index = 0; index < 6; index += 1) {
      normals.push(x, y, z);
    }
  }

  private pushFaceColor(colors: number[], color: any, shade: number): void {
    const red = color.r * shade;
    const green = color.g * shade;
    const blue = color.b * shade;

    for (let index = 0; index < 6; index += 1) {
      colors.push(red, green, blue);
    }
  }

  private resolveNeighborHeight(topology: NormalizedTopology, x: number, z: number): number {
    if (x < 0 || z < 0 || x >= topology.width || z >= topology.depth) {
      return 0;
    }

    const rawHeight = topology.heights[z][x];
    return rawHeight > 0 ? this.resolveScaledHeight(topology, rawHeight) : 0;
  }

  private resolveScaledHeight(topology: NormalizedTopology, rawHeight: number): number {
    return Math.max(
      0.22,
      rawHeight * topology.heightScale * MapComponent.VERTICAL_EXAGGERATION
    );
  }

  private resolveTerrainColor(
    topology: NormalizedTopology,
    x: number,
    z: number,
    rawHeight: number
  ): any {
    return this.terrainColorEnabled
      ? this.resolveVoxelColor(topology, x, z, rawHeight)
      : this.resolveMonochromeColor(topology, x, z, rawHeight);
  }

  private updateChunkVisibility(): void {
    if (!this.camera || !this.activeTopology || !this.terrainChunks.length) {
      return;
    }

    this.camera.updateMatrixWorld();
    this.cullingMatrix.multiplyMatrices(this.camera.projectionMatrix, this.camera.matrixWorldInverse);
    this.cullingFrustum.setFromProjectionMatrix(this.cullingMatrix);

    this.terrainChunks.forEach((chunk: TerrainChunk) => {
      this.cullingSphere.center.copy(chunk.center);
      this.cullingSphere.radius = chunk.radius;
      chunk.mesh.visible = this.cullingFrustum.intersectsSphere(this.cullingSphere);
    });
  }

  private scheduleRender(): void {
    if (this.frameId !== undefined) {
      return;
    }

    this.ngZone.runOutsideAngular(() => {
      this.frameId = window.requestAnimationFrame(() => {
        this.frameId = undefined;
        this.renderFrame();
      });
    });
  }

  private renderFrame(): void {
    if (!this.renderer || !this.scene || !this.camera) {
      return;
    }

    this.updateChunkVisibility();
    this.renderer.render(this.scene, this.camera);
  }

  private resizeRenderer(): void {
    if (!this.renderer || !this.camera || !this.canvasHost) {
      return;
    }

    const host = this.canvasHost.nativeElement;
    const width = host.clientWidth || 960;
    const height = host.clientHeight || 560;

    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
    this.scheduleRender();
  }

  private updateKeyLightPosition(): void {
    if (!this.keyLight) {
      return;
    }

    const radians = (this.lightRotation * Math.PI) / 180;
    this.keyLight.position.set(
      Math.cos(radians) * this.keyLightRadius,
      this.keyLightHeight,
      Math.sin(radians) * this.keyLightRadius
    );
  }

  private invalidateShadows(): void {
    if (this.renderer?.shadowMap) {
      this.renderer.shadowMap.needsUpdate = true;
    }
  }

  private applyLightingLevels(): void {
    const normalized = Math.min(1, Math.max(0, this.lightBrightness / 100));
    const directionalIntensity = 0.95 + Math.pow(normalized, 1.1) * 3.6;
    const ambientIntensity = 0.38 + normalized * 0.28;
    const fillIntensity = 0.18 + normalized * 0.2;

    if (this.keyLight) {
      this.keyLight.intensity = directionalIntensity;
    }

    if (this.ambientLight) {
      this.ambientLight.intensity = ambientIntensity;
    }

    if (this.fillLight) {
      this.fillLight.intensity = fillIntensity;
    }

    if (this.renderer) {
      this.renderer.toneMappingExposure = 0.98 + normalized * 0.22;
    }
  }

  private normalizeTopology(topology: CartographTopology): NormalizedTopology | null {
    let heights: number[][];
    if (Array.isArray(topology.tiles) && topology.tiles.length > 0) {
      const normalized = this.normalizeTileTopology(topology.tiles, topology);
      if (!normalized) {
        return null;
      }
      heights = normalized.heights;
    } else {
      const heightsSource: number[][] | number[] | undefined = topology.heights || topology.grid;
      if (!heightsSource) {
        return null;
      }

      if (Array.isArray(heightsSource[0])) {
        heights = (heightsSource as number[][]).map((row: number[]) => row.map((value: number) => Number(value) || 0));
      } else {
        const width = topology.width || topology.dimensions?.width;
        const depth = topology.depth || topology.dimensions?.depth;
        if (!width || !depth) {
          return null;
        }

        heights = [];
        const flat = heightsSource as number[];
        for (let z = 0; z < depth; z += 1) {
          heights.push(flat.slice(z * width, z * width + width).map((value: number) => Number(value) || 0));
        }
      }
    }

    const depth = heights.length;
    const width = depth ? heights[0].length : 0;
    if (!width || heights.some((row: number[]) => row.length !== width)) {
      return null;
    }

    const maxHeightValue = heights.reduce((maxValue: number, row: number[]) => {
      return Math.max(maxValue, ...row);
    }, 0);
    const voxelCount = heights.reduce((count: number, row: number[]) => {
      return count + row.filter((value: number) => value > 0).length;
    }, 0);
    const cellSize = topology.cellSize || 1;
    const heightScale = topology.verticalScale || topology.heightScale || 0.18;
    const declaredMaxHeight = Number(topology.maxHeight) || 0;
    const effectiveMaxHeightValue = Math.max(maxHeightValue, declaredMaxHeight);
    const maxHeight = Math.max(
      0.5,
      effectiveMaxHeightValue * heightScale * MapComponent.VERTICAL_EXAGGERATION
    );
    const maxSpan = Math.max(width * cellSize, depth * cellSize);

    return {
      width,
      depth,
      heights,
      cellSize,
      heightScale,
      maxHeight,
      maxHeightValue: effectiveMaxHeightValue,
      voxelCount,
      maxSpan,
      halfWidth: (width - 1) / 2,
      halfDepth: (depth - 1) / 2
    };
  }

  private normalizeTileTopology(tiles: CartographTile[], topology: CartographTopology): { heights: number[][] } | null {
    const derivedWidth = tiles.reduce((maxValue: number, tile: CartographTile) => Math.max(maxValue, tile.x), 0) + 1;
    const derivedDepth = tiles.reduce((maxValue: number, tile: CartographTile) => Math.max(maxValue, tile.z), 0) + 1;
    const width = topology.width || topology.dimensions?.width || derivedWidth;
    const depth = topology.depth || topology.dimensions?.depth || derivedDepth;

    if (!width || !depth) {
      return null;
    }

    const heights = Array.from({ length: depth }, () => Array(width).fill(0));
    tiles.forEach((tile: CartographTile) => {
      if (tile.x < 0 || tile.z < 0 || tile.x >= width || tile.z >= depth) {
        return;
      }

      heights[tile.z][tile.x] = Number(tile.height) || 0;
    });

    return { heights };
  }

  private resolveVoxelColor(
    topology: NormalizedTopology,
    x: number,
    z: number,
    rawHeight: number
  ): any {
    const normalizedHeight = topology.maxHeightValue > 0
      ? Math.min(1, Math.max(0, rawHeight / topology.maxHeightValue))
      : 0;

    const colorStops = [
      { stop: 0, color: new THREE.Color('#5b84c7') },
      { stop: 0.36, color: new THREE.Color('#749f5d') },
      { stop: 0.7, color: new THREE.Color('#d5bc63') },
      { stop: 1, color: new THREE.Color('#b66b53') }
    ];

    for (let index = 1; index < colorStops.length; index += 1) {
      const currentStop = colorStops[index];
      if (normalizedHeight > currentStop.stop) {
        continue;
      }

      const previousStop = colorStops[index - 1];
      const range = currentStop.stop - previousStop.stop || 1;
      const mix = (normalizedHeight - previousStop.stop) / range;
      return this.applyColorGrain(previousStop.color.clone().lerp(currentStop.color, mix), x, z);
    }

    return this.applyColorGrain(colorStops[colorStops.length - 1].color.clone(), x, z);
  }

  private resolveMonochromeColor(
    topology: NormalizedTopology,
    x: number,
    z: number,
    rawHeight: number
  ): any {
    const normalizedHeight = topology.maxHeightValue > 0
      ? Math.min(1, Math.max(0, rawHeight / topology.maxHeightValue))
      : 0;
    const intensity = 0.36 + normalizedHeight * 0.4;
    return this.applyMonochromeGrain(new THREE.Color(intensity, intensity, intensity), x, z);
  }

  private applyColorGrain(color: any, x: number, z: number): any {
    const noise = this.hashNoise(x, z);
    const brightness = 0.94 + noise * 0.14;
    const saturation = 0.94 + noise * 0.08;
    const hsl = { h: 0, s: 0, l: 0 };

    color.getHSL(hsl);
    color.setHSL(
      hsl.h,
      Math.min(1, hsl.s * saturation),
      Math.min(1, Math.max(0, hsl.l * brightness))
    );

    return color;
  }

  private applyMonochromeGrain(color: any, x: number, z: number): any {
    const noise = this.hashNoise(x, z);
    const brightness = 0.96 + noise * 0.1;
    const hsl = { h: 0, s: 0, l: 0 };

    color.getHSL(hsl);
    color.setHSL(
      hsl.h,
      hsl.s,
      Math.min(1, Math.max(0, hsl.l * brightness))
    );

    return color;
  }

  private hashNoise(x: number, z: number): number {
    const value = Math.sin((x + 1) * 12.9898 + (z + 1) * 78.233) * 43758.5453;
    return value - Math.floor(value);
  }

  private disposeScene(): void {
    if (this.frameId !== undefined) {
      window.cancelAnimationFrame(this.frameId);
      this.frameId = undefined;
    }

    this.disposables.forEach((resource: any) => {
      if (resource && typeof resource.dispose === 'function') {
        resource.dispose();
      }
    });
    this.disposables = [];

    if (this.controls) {
      this.controls.dispose();
      this.controls = undefined;
    }

    this.keyLight = undefined;
    this.ambientLight = undefined;
    this.fillLight = undefined;
    this.terrainChunks = [];
    this.activeTopology = undefined;
    this.keyLightRadius = 0;
    this.keyLightHeight = 0;

    if (this.scene) {
      this.scene.clear();
    }

    if (this.renderer) {
      this.renderer.dispose();
      this.renderer = undefined;
    }

    if (this.canvasHost) {
      this.canvasHost.nativeElement.innerHTML = '';
    }
  }

  private trackDisposable(...resources: any[]): void {
    resources.forEach((resource: any) => {
      if (resource) {
        this.disposables.push(resource);
      }
    });
  }
}

interface NormalizedTopology {
  width: number;
  depth: number;
  heights: number[][];
  cellSize: number;
  heightScale: number;
  maxHeight: number;
  maxHeightValue: number;
  voxelCount: number;
  maxSpan: number;
  halfWidth: number;
  halfDepth: number;
}

interface MapStats {
  format: string;
  tileCount: number;
  width: number;
  depth: number;
  maxHeight: number;
}

interface TerrainChunk {
  mesh: any;
  startX: number;
  endX: number;
  startZ: number;
  endZ: number;
  center: any;
  radius: number;
}

import { Injectable } from '@angular/core';
import { HttpBackend, HttpClient } from '@angular/common/http';
import { Observable, throwError } from 'rxjs';
import { catchError, map } from 'rxjs/operators';

export interface CartographTopology {
  version?: number;
  format?: string;
  name?: string;
  source?: string;
  width?: number;
  depth?: number;
  dimensions?: {
    width?: number;
    depth?: number;
  };
  maxHeight?: number;
  cellSize?: number;
  heightScale?: number;
  verticalScale?: number;
  tiles?: CartographTile[];
  heights?: number[][] | number[];
  grid?: number[][];
}

export interface CartographTile {
  x: number;
  z: number;
  height: number;
}

export interface CartographMapResponse {
  status: 'ok' | 'missing_topology' | 'error';
  message?: string;
  topologyAvailable: boolean;
  topology?: CartographTopology;
  source?: {
    directory?: string;
    topologyFile?: string;
    transport?: 'backend' | 'asset';
  };
}

@Injectable({
  providedIn: 'root'
})
export class MapService {
  private readonly rawHttp: HttpClient;

  constructor(
    private readonly http: HttpClient,
    httpBackend: HttpBackend
  ) {
    this.rawHttp = new HttpClient(httpBackend);
  }

  public loadMap(): Observable<CartographMapResponse> {
    return this.http.get<CartographMapResponse>('/cartograph/map').pipe(
      catchError(() => this.loadAssetTopology())
    );
  }

  private loadAssetTopology(): Observable<CartographMapResponse> {
    return this.rawHttp.get<CartographTopology>('/cartograph-map/map.json').pipe(
      map((topology: CartographTopology) => ({
        status: 'ok' as const,
        topologyAvailable: true,
        topology,
        source: {
          directory: 'cartograph-map',
          topologyFile: 'map.json',
          transport: 'asset' as const
        }
      })),
      catchError(() => {
        return this.rawHttp.get<CartographTopology>('/cartograph-map/topology.json').pipe(
          map((topology: CartographTopology) => ({
            status: 'ok' as const,
            topologyAvailable: true,
            topology,
            source: {
              directory: 'cartograph-map',
              topologyFile: 'topology.json',
              transport: 'asset' as const
            }
          })),
          catchError(() => {
            return throwError(() => new Error('Map topology could not be loaded from backend or frontend assets.'));
          })
        );
      })
    );
  }
}

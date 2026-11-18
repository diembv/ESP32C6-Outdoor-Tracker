const DB_NAME = 'esp32-tracker-topo-tiles';
const STORE_NAME = 'tiles';
const DB_VERSION = 1;

export interface TileCoord {
  z: number;
  x: number;
  y: number;
}

export interface TileCacheStats {
  tileCount: number;
  estimatedMb: number;
}

function openTileDb(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = () => {
      const db = req.result;
      if (!db.objectStoreNames.contains(STORE_NAME)) {
        db.createObjectStore(STORE_NAME);
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

export async function getCachedTile(key: string): Promise<ArrayBuffer | null> {
  try {
    const db = await openTileDb();
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(STORE_NAME, 'readonly');
      const req = tx.objectStore(STORE_NAME).get(key);
      req.onsuccess = () => resolve((req.result as ArrayBuffer) || null);
      req.onerror = () => reject(req.error);
    });
  } catch {
    return null;
  }
}

export async function putCachedTile(key: string, data: ArrayBuffer): Promise<void> {
  try {
    const db = await openTileDb();
    await new Promise<void>((resolve, reject) => {
      const tx = db.transaction(STORE_NAME, 'readwrite');
      tx.objectStore(STORE_NAME).put(data, key);
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(tx.error);
    });
  } catch {
    // Ignore storage quota errors
  }
}

export async function getTileCacheStats(): Promise<TileCacheStats> {
  try {
    const db = await openTileDb();
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(STORE_NAME, 'readonly');
      const req = tx.objectStore(STORE_NAME).count();
      req.onsuccess = () => {
        const count = req.result;
        resolve({
          tileCount: count,
          estimatedMb: Number(((count * 22) / 1024).toFixed(2)),
        });
      };
      req.onerror = () => reject(req.error);
    });
  } catch {
    return { tileCount: 0, estimatedMb: 0 };
  }
}

export async function clearTileCache(): Promise<void> {
  const db = await openTileDb();
  await new Promise<void>((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, 'readwrite');
    tx.objectStore(STORE_NAME).clear();
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}

function lon2tile(lon: number, zoom: number): number {
  return Math.floor(((lon + 180) / 360) * Math.pow(2, zoom));
}

function lat2tile(lat: number, zoom: number): number {
  const rad = (lat * Math.PI) / 180;
  return Math.floor(
    ((1 - Math.log(Math.tan(rad) + 1 / Math.cos(rad)) / Math.PI) / 2) * Math.pow(2, zoom)
  );
}

/**
 * Compute all XYZ tiles inside a bounding box + bufferKm (default 2km) for zoom levels.
 */
export function getTilesForBounds(
  minLat: number,
  maxLat: number,
  minLon: number,
  maxLon: number,
  minZoom = 12,
  maxZoom = 15,
  bufferKm = 2.0
): TileCoord[] {
  const dLat = bufferKm / 111.32;
  const dLon = bufferKm / (111.32 * Math.cos((((minLat + maxLat) / 2) * Math.PI) / 180));

  const sLat = Math.max(-85, minLat - dLat);
  const nLat = Math.min(85, maxLat + dLat);
  const wLon = Math.max(-180, minLon - dLon);
  const eLon = Math.min(180, maxLon + dLon);

  const tiles: TileCoord[] = [];
  for (let z = minZoom; z <= maxZoom; z++) {
    const xMin = lon2tile(wLon, z);
    const xMax = lon2tile(eLon, z);
    const yMin = lat2tile(nLat, z);
    const yMax = lat2tile(sLat, z);
    for (let x = xMin; x <= xMax; x++) {
      for (let y = yMin; y <= yMax; y++) {
        tiles.push({ z, x, y });
      }
    }
  }
  return tiles;
}

/**
 * Download tiles into IndexedDB with concurrency limit & progress callback.
 */
export async function downloadTilesForRoute(
  tiles: TileCoord[],
  onProgress: (done: number, total: number, skipped: number) => void,
  signal?: AbortSignal
): Promise<{ downloaded: number; skipped: number }> {
  let done = 0;
  let skipped = 0;
  const concurrency = 4;
  let idx = 0;

  async function worker() {
    while (idx < tiles.length) {
      if (signal?.aborted) return;
      const tile = tiles[idx++];
      const key = `${tile.z}/${tile.x}/${tile.y}`;
      const existing = await getCachedTile(key);
      if (existing) {
        skipped++;
        done++;
        onProgress(done, tiles.length, skipped);
        continue;
      }

      try {
        const url = `https://tile.opentopomap.org/${tile.z}/${tile.x}/${tile.y}.png`;
        const res = await fetch(url, { signal });
        if (res.ok) {
          const buf = await res.arrayBuffer();
          await putCachedTile(key, buf);
        }
      } catch {
        // Skip failed tile
      }
      done++;
      onProgress(done, tiles.length, skipped);
    }
  }

  const workers = Array.from({ length: concurrency }, () => worker());
  await Promise.all(workers);
  return { downloaded: done - skipped, skipped };
}

/**
 * Fetch tile from IndexedDB first, falling back to OpenTopoMap network and caching it.
 */
export async function fetchOfflineFirstTile(z: number, x: number, y: number): Promise<ArrayBuffer> {
  const key = `${z}/${x}/${y}`;
  const cached = await getCachedTile(key);
  if (cached) return cached;

  const url = `https://tile.opentopomap.org/${z}/${x}/${y}.png`;
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Tile ${key} HTTP ${res.status}`);
  const buf = await res.arrayBuffer();
  await putCachedTile(key, buf);
  return buf;
}

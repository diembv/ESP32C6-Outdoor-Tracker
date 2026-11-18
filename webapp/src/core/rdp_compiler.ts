import type { CompiledRoute, GeoPoint, RouteHeader, RouteWaypoint, WaypointIcon } from './types';

export const ROUTE_HEADER_MAGIC = 'ROUT';
export const ROUTE_HEADER_SIZE = 96;
export const ROUTE_POINT_SIZE = 8;
export const ROUTE_WAYPOINT_SIZE = 32;

const DEG_TO_RAD = Math.PI / 180;
const RAD_TO_DEG = 180 / Math.PI;
const EARTH_RADIUS_M = 6371000.0;

/**
 * Haversine distance between two GPS coordinates in meters.
 */
export function haversineDistanceM(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const phi1 = lat1 * DEG_TO_RAD;
  const phi2 = lat2 * DEG_TO_RAD;
  const dPhi = (lat2 - lat1) * DEG_TO_RAD;
  const dLambda = (lon2 - lon1) * DEG_TO_RAD;
  const a =
    Math.sin(dPhi / 2) * Math.sin(dPhi / 2) +
    Math.cos(phi1) * Math.cos(phi2) * Math.sin(dLambda / 2) * Math.sin(dLambda / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return EARTH_RADIUS_M * c;
}

/**
 * Bearing from point 1 to point 2 in degrees (0 - 360).
 */
export function bearingDeg(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const phi1 = lat1 * DEG_TO_RAD;
  const phi2 = lat2 * DEG_TO_RAD;
  const dLambda = (lon2 - lon1) * DEG_TO_RAD;
  const y = Math.sin(dLambda) * Math.cos(phi2);
  const x = Math.cos(phi1) * Math.sin(phi2) - Math.sin(phi1) * Math.cos(phi2) * Math.cos(dLambda);
  return ((Math.atan2(y, x) * RAD_TO_DEG) + 360) % 360;
}

/**
 * Perpendicular distance (meters) from pt to segment (lineStart, lineEnd)
 * using local Equirectangular projection (matches tools/gpx_to_route.py).
 */
export function perpendicularDistanceM(
  pt: GeoPoint,
  lineStart: GeoPoint,
  lineEnd: GeoPoint
): number {
  const midLat = ((lineStart.lat + lineEnd.lat + pt.lat) / 3.0) * DEG_TO_RAD;
  const kx = 111320.0 * Math.cos(midLat);
  const ky = 111320.0;

  const x = (pt.lon - lineStart.lon) * kx;
  const y = (pt.lat - lineStart.lat) * ky;

  const x2 = (lineEnd.lon - lineStart.lon) * kx;
  const y2 = (lineEnd.lat - lineStart.lat) * ky;

  const lineLenSq = x2 * x2 + y2 * y2;
  if (lineLenSq === 0) {
    return Math.sqrt(x * x + y * y);
  }

  const t = Math.max(0.0, Math.min(1.0, (x * x2 + y * y2) / lineLenSq));
  const projX = t * x2;
  const projY = t * y2;
  const dx = x - projX;
  const dy = y - projY;
  return Math.sqrt(dx * dx + dy * dy);
}

/**
 * Iterative Ramer-Douglas-Peucker polyline simplification (avoids stack overflow on huge GPX files).
 */
export function ramerDouglasPeucker(points: GeoPoint[], epsilonMeters: number): GeoPoint[] {
  if (points.length < 3 || epsilonMeters <= 0) {
    return points.slice();
  }

  const keep = new Uint8Array(points.length);
  keep[0] = 1;
  keep[points.length - 1] = 1;

  const stack: [number, number][] = [[0, points.length - 1]];
  while (stack.length > 0) {
    const [start, end] = stack.pop()!;
    if (end - start < 2) continue;

    let dmax = 0;
    let index = start;
    for (let i = start + 1; i < end; i++) {
      const d = perpendicularDistanceM(points[i], points[start], points[end]);
      if (d > dmax) {
        dmax = d;
        index = i;
      }
    }

    if (dmax > epsilonMeters) {
      keep[index] = 1;
      stack.push([start, index], [index, end]);
    }
  }

  const result: GeoPoint[] = [];
  for (let i = 0; i < points.length; i++) {
    if (keep[i]) result.push(points[i]);
  }
  return result;
}

/**
 * Parse GPX XML string into track points, route name, and optional waypoints.
 * Works both in Browser (DOMParser) and Node/Vitest (regex fallback if DOMParser unavailable).
 */
export function parseGpx(xmlString: string): {
  name: string;
  points: GeoPoint[];
  waypoints: RouteWaypoint[];
} {
  let name = 'Trekking Route';
  const points: GeoPoint[] = [];
  const waypoints: RouteWaypoint[] = [];

  if (typeof DOMParser !== 'undefined') {
    const doc = new DOMParser().parseFromString(xmlString, 'application/xml');
    const nameEl =
      doc.querySelector('trk > name') ||
      doc.querySelector('rte > name') ||
      doc.querySelector('metadata > name') ||
      doc.querySelector('name');
    if (nameEl?.textContent?.trim()) {
      name = nameEl.textContent.trim();
    }

    const ptEls = doc.querySelectorAll('trkpt, rtept');
    ptEls.forEach((el) => {
      const lat = parseFloat(el.getAttribute('lat') || '');
      const lon = parseFloat(el.getAttribute('lon') || '');
      if (Number.isFinite(lat) && Number.isFinite(lon)) {
        const eleEl = el.querySelector('ele');
        const timeEl = el.querySelector('time');
        const ele = eleEl?.textContent ? parseFloat(eleEl.textContent) : undefined;
        const ts = timeEl?.textContent ? Math.floor(Date.parse(timeEl.textContent) / 1000) : undefined;
        points.push({
          lat,
          lon,
          ele: Number.isFinite(ele) ? ele : undefined,
          ts: Number.isFinite(ts) ? ts : undefined,
        });
      }
    });

    const wptEls = doc.querySelectorAll('wpt');
    wptEls.forEach((el) => {
      const lat = parseFloat(el.getAttribute('lat') || '');
      const lon = parseFloat(el.getAttribute('lon') || '');
      if (Number.isFinite(lat) && Number.isFinite(lon)) {
        const eleEl = el.querySelector('ele');
        const wNameEl = el.querySelector('name');
        const symEl = el.querySelector('sym, type');
        const alt_m = eleEl?.textContent ? Math.round(parseFloat(eleEl.textContent)) : 0;
        const label = (wNameEl?.textContent || 'WPT').trim().slice(0, 20);
        const sym = (symEl?.textContent || '').toLowerCase();
        let icon_type: WaypointIcon = 0;
        if (sym.includes('summit') || sym.includes('peak') || label.toLowerCase().includes('dinh')) icon_type = 1;
        else if (sym.includes('camp') || label.toLowerCase().includes('lan')) icon_type = 2;
        else if (sym.includes('water') || label.toLowerCase().includes('suoi') || label.toLowerCase().includes('thac')) icon_type = 3;
        else if (sym.includes('danger') || sym.includes('warn')) icon_type = 4;
        waypoints.push({ lat, lon, alt_m: Number.isFinite(alt_m) ? alt_m : 0, icon_type, label });
      }
    });
    return { name, points, waypoints };
  }

  // Lightweight regex fallback for Node unit tests
  const nameMatch = xmlString.match(/<name[^>]*>([^<]+)<\/name>/i);
  if (nameMatch?.[1]) name = nameMatch[1].trim();

  const ptRegex = /<(?:trkpt|rtept)\b([^>]*)>([\s\S]*?)<\/(?:trkpt|rtept)>|<(?:trkpt|rtept)\b([^>]*)\/>/gi;
  let m: RegExpExecArray | null;
  while ((m = ptRegex.exec(xmlString)) !== null) {
    const attrs = m[1] || m[3] || '';
    const body = m[2] || '';
    const latM = attrs.match(/lat=["']([^"']+)["']/i);
    const lonM = attrs.match(/lon=["']([^"']+)["']/i);
    if (!latM || !lonM) continue;
    const lat = parseFloat(latM[1]);
    const lon = parseFloat(lonM[1]);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
    const eleM = body.match(/<ele[^>]*>([^<]+)<\/ele>/i);
    const ele = eleM ? parseFloat(eleM[1]) : undefined;
    points.push({ lat, lon, ele: Number.isFinite(ele) ? ele : undefined });
  }
  return { name, points, waypoints };
}

/**
 * Strip Vietnamese diacritics to clean ASCII for embedded TFT/AMOLED display compatibility.
 */
export function toAsciiSafe(str: string): string {
  return str
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .replace(/đ/g, 'd')
    .replace(/Đ/g, 'D')
    .replace(/[^\x20-\x7E]/g, '');
}

/**
 * Compile route points & optional waypoints into ESP32 binary `.bin` format.
 * Matches RouteHeader (96B) + RoutePoint[N] (8B each) + RouteWaypoint[M] (32B each).
 */
export function compileRouteBin(
  routeName: string,
  rawPoints: GeoPoint[],
  waypoints: RouteWaypoint[] = [],
  epsilonMeters = 5.0
): CompiledRoute {
  const points = epsilonMeters > 0 ? ramerDouglasPeucker(rawPoints, epsilonMeters) : rawPoints.slice();
  if (points.length === 0) {
    throw new Error('Cannot compile an empty route');
  }

  let min_lat = Infinity;
  let max_lat = -Infinity;
  let min_lon = Infinity;
  let max_lon = -Infinity;
  let total_dist_m = 0;

  for (let i = 0; i < points.length; i++) {
    const p = points[i];
    if (p.lat < min_lat) min_lat = p.lat;
    if (p.lat > max_lat) max_lat = p.lat;
    if (p.lon < min_lon) min_lon = p.lon;
    if (p.lon > max_lon) max_lon = p.lon;
    if (i > 0) {
      total_dist_m += haversineDistanceM(points[i - 1].lat, points[i - 1].lon, p.lat, p.lon);
    }
  }

  const total_dist_km = total_dist_m / 1000.0;
  const version = waypoints.length > 0 ? 2 : 1;
  const cleanName = toAsciiSafe(routeName || 'Route').slice(0, 31);

  const totalBytes =
    ROUTE_HEADER_SIZE +
    points.length * ROUTE_POINT_SIZE +
    waypoints.length * ROUTE_WAYPOINT_SIZE;

  const buffer = new Uint8Array(totalBytes);
  const view = new DataView(buffer.buffer);
  const encoder = new TextEncoder();

  // magic[4] = "ROUT"
  for (let i = 0; i < 4; i++) {
    buffer[i] = ROUTE_HEADER_MAGIC.charCodeAt(i);
  }
  view.setUint16(4, version, true);
  view.setUint16(6, waypoints.length, true);
  view.setUint32(8, points.length, true);
  view.setFloat32(12, total_dist_km, true);
  view.setFloat32(16, min_lat, true);
  view.setFloat32(20, max_lat, true);
  view.setFloat32(24, min_lon, true);
  view.setFloat32(28, max_lon, true);

  const nameBytes = encoder.encode(cleanName);
  buffer.set(nameBytes.slice(0, 31), 32);
  // bytes 64..95 remain 0 (reserved[32])

  let offset = ROUTE_HEADER_SIZE;
  for (const pt of points) {
    view.setFloat32(offset, pt.lat, true);
    view.setFloat32(offset + 4, pt.lon, true);
    offset += ROUTE_POINT_SIZE;
  }

  for (const wpt of waypoints) {
    view.setFloat32(offset, wpt.lat, true);
    view.setFloat32(offset + 4, wpt.lon, true);
    view.setInt16(offset + 8, Math.round(wpt.alt_m), true);
    buffer[offset + 10] = wpt.icon_type;
    const lblBytes = encoder.encode(toAsciiSafe(wpt.label).slice(0, 20));
    buffer.set(lblBytes, offset + 11);
    offset += ROUTE_WAYPOINT_SIZE;
  }

  const header: RouteHeader = {
    magic: ROUTE_HEADER_MAGIC,
    version,
    waypoint_count: waypoints.length,
    point_count: points.length,
    total_dist_km,
    min_lat,
    max_lat,
    min_lon,
    max_lon,
    name: cleanName,
  };

  return { header, points, waypoints, buffer };
}

/**
 * Decode an ESP32 `.bin` route file back into header, points, and waypoints.
 */
export function parseRouteBin(buffer: Uint8Array): CompiledRoute {
  if (buffer.byteLength < ROUTE_HEADER_SIZE) {
    throw new Error('Invalid route binary: smaller than 96-byte header');
  }
  const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  const decoder = new TextDecoder();

  const magic = String.fromCharCode(buffer[0], buffer[1], buffer[2], buffer[3]);
  if (magic !== ROUTE_HEADER_MAGIC) {
    throw new Error(`Invalid route magic: "${magic}"`);
  }

  const version = view.getUint16(4, true);
  const waypoint_count = view.getUint16(6, true);
  const point_count = view.getUint32(8, true);
  const total_dist_km = view.getFloat32(12, true);
  const min_lat = view.getFloat32(16, true);
  const max_lat = view.getFloat32(20, true);
  const min_lon = view.getFloat32(24, true);
  const max_lon = view.getFloat32(28, true);

  const nameRaw = buffer.subarray(32, 64);
  const nullIdx = nameRaw.indexOf(0);
  const name = decoder.decode(nullIdx >= 0 ? nameRaw.subarray(0, nullIdx) : nameRaw);

  const points: GeoPoint[] = [];
  let offset = ROUTE_HEADER_SIZE;
  for (let i = 0; i < point_count; i++) {
    if (offset + ROUTE_POINT_SIZE > buffer.byteLength) break;
    points.push({
      lat: view.getFloat32(offset, true),
      lon: view.getFloat32(offset + 4, true),
    });
    offset += ROUTE_POINT_SIZE;
  }

  const waypoints: RouteWaypoint[] = [];
  if (version >= 2) {
    for (let i = 0; i < waypoint_count; i++) {
      if (offset + ROUTE_WAYPOINT_SIZE > buffer.byteLength) break;
      const lblRaw = buffer.subarray(offset + 11, offset + 32);
      const lblNull = lblRaw.indexOf(0);
      waypoints.push({
        lat: view.getFloat32(offset, true),
        lon: view.getFloat32(offset + 4, true),
        alt_m: view.getInt16(offset + 8, true),
        icon_type: buffer[offset + 10] as WaypointIcon,
        label: decoder.decode(lblNull >= 0 ? lblRaw.subarray(0, lblNull) : lblRaw),
      });
      offset += ROUTE_WAYPOINT_SIZE;
    }
  }

  return {
    header: {
      magic,
      version,
      waypoint_count,
      point_count,
      total_dist_km,
      min_lat,
      max_lat,
      min_lon,
      max_lon,
      name,
    },
    points,
    waypoints,
    buffer,
  };
}

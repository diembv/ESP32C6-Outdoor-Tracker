import { haversineDistanceM } from './rdp_compiler';
import type { GeoPoint } from './types';

export interface TrackPoint extends GeoPoint {
  ts: number; // Unix epoch seconds
  ele: number;
}

export interface TrackStats {
  pointCount: number;
  startTimeIso: string;
  endTimeIso: string;
  totalDurationS: number;
  movingTimeS: number;
  stoppedTimeS: number;
  totalDistKm: number;
  elevationGainM: number;
  elevationLossM: number;
  minAltM: number;
  maxAltM: number;
  avgVamMh: number; // Vertical Ascent Meters/hour
  avgSpeedKmh: number;
}

/**
 * Parse Compact CSV (ts,lat,lon,alt) from ESP32 track_logger.cpp.
 */
export function parseCompactCsv(csvText: string): TrackPoint[] {
  const lines = csvText.split(/\r?\n/);
  const points: TrackPoint[] = [];
  let colIdx = { ts: 0, lat: 1, lon: 2, alt: 3 };
  let checkedHeader = false;

  for (const rawLine of lines) {
    const line = rawLine.trim();
    if (!line) continue;
    const cols = line.split(',').map((c) => c.trim());

    if (!checkedHeader) {
      checkedHeader = true;
      const lower = cols.map((c) => c.toLowerCase());
      if (lower.includes('ts') || lower.includes('lat')) {
        colIdx = {
          ts: lower.indexOf('ts'),
          lat: lower.indexOf('lat'),
          lon: lower.indexOf('lon'),
          alt: lower.indexOf('alt'),
        };
        continue;
      }
    }

    if (cols.length < 3) continue;
    const ts = parseFloat(cols[colIdx.ts]);
    const lat = parseFloat(cols[colIdx.lat]);
    const lon = parseFloat(cols[colIdx.lon]);
    const alt = colIdx.alt >= 0 && cols[colIdx.alt] !== undefined ? parseFloat(cols[colIdx.alt]) : 0;

    if (
      !Number.isFinite(ts) ||
      !Number.isFinite(lat) ||
      !Number.isFinite(lon) ||
      lat < -90 ||
      lat > 90 ||
      lon < -180 ||
      lon > 180
    ) {
      continue;
    }

    points.push({
      ts,
      lat,
      lon,
      ele: Number.isFinite(alt) ? alt : 0,
    });
  }

  return points;
}

/**
 * Compute detailed trekking analytics (Moving Time, Elevation Gain/Loss, VAM).
 */
export function computeTrackStats(points: TrackPoint[]): TrackStats {
  if (points.length === 0) {
    return {
      pointCount: 0,
      startTimeIso: '',
      endTimeIso: '',
      totalDurationS: 0,
      movingTimeS: 0,
      stoppedTimeS: 0,
      totalDistKm: 0,
      elevationGainM: 0,
      elevationLossM: 0,
      minAltM: 0,
      maxAltM: 0,
      avgVamMh: 0,
      avgSpeedKmh: 0,
    };
  }

  let totalDistM = 0;
  let movingTimeS = 0;
  let elevationGainM = 0;
  let elevationLossM = 0;
  let minAltM = points[0].ele;
  let maxAltM = points[0].ele;
  let ascentTimeS = 0;

  // Smooth elevation slightly (3-point moving average) to avoid baro/GPS micro-jitter
  const smoothedEle = points.map((p, i) => {
    const prev = points[Math.max(0, i - 1)].ele;
    const next = points[Math.min(points.length - 1, i + 1)].ele;
    return (prev + p.ele + next) / 3;
  });

  for (let i = 0; i < points.length; i++) {
    const p = points[i];
    if (p.ele < minAltM) minAltM = p.ele;
    if (p.ele > maxAltM) maxAltM = p.ele;

    if (i > 0) {
      const prev = points[i - 1];
      const dt = Math.max(0, p.ts - prev.ts);
      const dist = haversineDistanceM(prev.lat, prev.lon, p.lat, p.lon);
      totalDistM += dist;

      // Speed > 0.35 m/s (~1.26 km/h) counts as moving
      if (dt > 0 && dt < 300 && dist / dt > 0.35) {
        movingTimeS += dt;
      }

      const dEle = smoothedEle[i] - smoothedEle[i - 1];
      if (dEle > 0.5) {
        elevationGainM += dEle;
        if (dt > 0 && dt < 300) ascentTimeS += dt;
      } else if (dEle < -0.5) {
        elevationLossM += Math.abs(dEle);
      }
    }
  }

  const totalDurationS = Math.max(0, Math.round(points[points.length - 1].ts - points[0].ts));
  const stoppedTimeS = Math.max(0, totalDurationS - Math.round(movingTimeS));
  const totalDistKm = totalDistM / 1000.0;
  const avgVamMh = ascentTimeS > 60 ? Math.round((elevationGainM / ascentTimeS) * 3600) : 0;
  const avgSpeedKmh = movingTimeS > 0 ? (totalDistKm / (movingTimeS / 3600)) : 0;

  return {
    pointCount: points.length,
    startTimeIso: new Date(points[0].ts * 1000).toISOString().replace(/\.\d{3}Z$/, 'Z'),
    endTimeIso: new Date(points[points.length - 1].ts * 1000).toISOString().replace(/\.\d{3}Z$/, 'Z'),
    totalDurationS,
    movingTimeS: Math.round(movingTimeS),
    stoppedTimeS,
    totalDistKm,
    elevationGainM: Math.round(elevationGainM),
    elevationLossM: Math.round(elevationLossM),
    minAltM: Math.round(minAltM),
    maxAltM: Math.round(maxAltM),
    avgVamMh,
    avgSpeedKmh,
  };
}

/**
 * Convert Compact CSV text to standard GPX 1.1 XML string (compatible with Strava / Garmin Connect).
 */
export function convertCsvToGpx(
  csvText: string,
  customTrackName?: string
): { gpxXml: string; stats: TrackStats; points: TrackPoint[] } {
  const points = parseCompactCsv(csvText);
  if (points.length === 0) {
    throw new Error('Không tìm thấy điểm tọa độ hợp lệ trong file CSV');
  }

  const stats = computeTrackStats(points);
  const dtStr = stats.startTimeIso.slice(0, 19).replace(/[-:]/g, '').replace('T', '_');
  const trackName = customTrackName || `Track ${dtStr}`;

  const lines: string[] = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    '<gpx version="1.1" creator="ESP32-C6 Outdoor Tracker Web App"',
    '     xmlns="http://www.topografix.com/GPX/1/1"',
    '     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"',
    '     xsi:schemaLocation="http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd">',
    '  <metadata>',
    `    <name>${trackName}</name>`,
    `    <time>${stats.startTimeIso}</time>`,
    '  </metadata>',
    '  <trk>',
    `    <name>${trackName}</name>`,
    '    <trkseg>',
  ];

  for (const pt of points) {
    const iso = new Date(pt.ts * 1000).toISOString().replace(/\.\d{3}Z$/, 'Z');
    lines.push(`      <trkpt lat="${pt.lat.toFixed(6)}" lon="${pt.lon.toFixed(6)}">`);
    lines.push(`        <ele>${pt.ele.toFixed(1)}</ele>`);
    lines.push(`        <time>${iso}</time>`);
    lines.push('      </trkpt>');
  }

  lines.push('    </trkseg>', '  </trk>', '</gpx>', '');

  return {
    gpxXml: lines.join('\n'),
    stats,
    points,
  };
}

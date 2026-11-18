import { describe, it, expect } from 'vitest';
import {
  compileRouteBin,
  parseRouteBin,
  parseGpx,
  ramerDouglasPeucker,
  haversineDistanceM,
} from './rdp_compiler';
import { convertCsvToGpx, parseCompactCsv, computeTrackStats } from './csv_to_gpx';
import { SAMPLE_BAVI_GPX } from './demo_data';

describe('rdp_compiler', () => {
  it('parses GPX points and waypoints', () => {
    const parsed = parseGpx(SAMPLE_BAVI_GPX);
    expect(parsed.name).toBe('Ba Vi Summit Trail');
    expect(parsed.points.length).toBe(13);
    expect(parsed.points[0].lat).toBeCloseTo(21.07, 4);
  });

  it('compiles and decodes RouteHeader v1 (96 bytes + N*8 bytes)', () => {
    const pts = [
      { lat: 21.07, lon: 105.362 },
      { lat: 21.0725, lon: 105.3638 },
      { lat: 21.0748, lon: 105.3652 },
    ];
    const compiled = compileRouteBin('Đền Thượng', pts, [], 0);
    expect(compiled.header.magic).toBe('ROUT');
    expect(compiled.header.version).toBe(1);
    expect(compiled.header.point_count).toBe(3);
    expect(compiled.header.name).toBe('Den Thuong');
    expect(compiled.buffer.byteLength).toBe(96 + 3 * 8);

    const decoded = parseRouteBin(compiled.buffer);
    expect(decoded.header.version).toBe(1);
    expect(decoded.points.length).toBe(3);
    expect(decoded.points[2].lat).toBeCloseTo(21.0748, 4);
  });

  it('compiles and decodes RouteHeader v2 with 32-byte Waypoints', () => {
    const pts = [
      { lat: 22.618, lon: 103.571 },
      { lat: 22.6285, lon: 103.5815 },
      { lat: 22.6338, lon: 103.587 },
    ];
    const wpts = [
      { lat: 22.6285, lon: 103.5815, alt_m: 2400, icon_type: 2 as const, label: 'LAN NGHI 2400M' },
      { lat: 22.6338, lon: 103.587, alt_m: 2965, icon_type: 1 as const, label: 'DINH NHIU CO SAN' },
    ];
    const compiled = compileRouteBin('Nhiu Co San', pts, wpts, 0);
    expect(compiled.header.version).toBe(2);
    expect(compiled.header.waypoint_count).toBe(2);
    expect(compiled.buffer.byteLength).toBe(96 + 3 * 8 + 2 * 32);

    const decoded = parseRouteBin(compiled.buffer);
    expect(decoded.waypoints.length).toBe(2);
    expect(decoded.waypoints[0].alt_m).toBe(2400);
    expect(decoded.waypoints[0].icon_type).toBe(2);
    expect(decoded.waypoints[0].label).toBe('LAN NGHI 2400M');
    expect(decoded.waypoints[1].label).toBe('DINH NHIU CO SAN');
  });

  it('simplifies collinear points with RDP', () => {
    const line = [
      { lat: 21.0, lon: 105.0 },
      { lat: 21.001, lon: 105.001 },
      { lat: 21.002, lon: 105.002 },
      { lat: 21.003, lon: 105.003 },
    ];
    const simplified = ramerDouglasPeucker(line, 5.0);
    expect(simplified.length).toBe(2);
    expect(haversineDistanceM(21.0, 105.0, 21.003, 105.003)).toBeGreaterThan(400);
  });
});

describe('csv_to_gpx', () => {
  it('converts compact CSV to GPX 1.1 and computes elevation/VAM stats', () => {
    const csv = [
      'ts,lat,lon,alt',
      '1763364600,21.070000,105.362000,400.0',
      '1763364900,21.072000,105.363500,520.0',
      '1763365200,21.074000,105.365000,640.0',
    ].join('\n');

    const pts = parseCompactCsv(csv);
    expect(pts.length).toBe(3);

    const stats = computeTrackStats(pts);
    expect(stats.totalDurationS).toBe(600);
    expect(stats.elevationGainM).toBeGreaterThan(150);
    expect(stats.maxAltM).toBe(640);

    const { gpxXml } = convertCsvToGpx(csv, 'BaVi Test');
    expect(gpxXml).toContain('<gpx version="1.1"');
    expect(gpxXml).toContain('<name>BaVi Test</name>');
    expect(gpxXml).toContain('<trkpt lat="21.070000" lon="105.362000">');
    expect(gpxXml).toContain('<ele>400.0</ele>');
  });
});

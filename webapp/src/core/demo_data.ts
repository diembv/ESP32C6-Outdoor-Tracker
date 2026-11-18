/**
 * Sample GPX route (Đền Thượng - Ba Vì Summit) & 3-member Nhìu Cồ San Expedition CSVs
 * for immediate testing in Route Studio and Team Replay.
 */

export const SAMPLE_BAVI_GPX = `<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="ESP32C6-Outdoor-Tracker">
  <metadata><name>Ba Vi Summit Trail</name></metadata>
  <wpt lat="21.07000" lon="105.36200"><ele>410</ele><name>CONG VUON QG</name><sym>Camp</sym></wpt>
  <wpt lat="21.07250" lon="105.36380"><ele>690</ele><name>SUOI NGOC</name><sym>Water</sym></wpt>
  <wpt lat="21.07480" lon="105.36520"><ele>1126</ele><name>DEN THUONG 1126M</name><sym>Summit</sym></wpt>
  <trk>
    <name>Ba Vi Summit</name>
    <trkseg>
      <trkpt lat="21.07000" lon="105.36200"><ele>410</ele></trkpt>
      <trkpt lat="21.07045" lon="105.36225"><ele>442</ele></trkpt>
      <trkpt lat="21.07090" lon="105.36260"><ele>485</ele></trkpt>
      <trkpt lat="21.07135" lon="105.36295"><ele>530</ele></trkpt>
      <trkpt lat="21.07180" lon="105.36320"><ele>582</ele></trkpt>
      <trkpt lat="21.07215" lon="105.36350"><ele>634</ele></trkpt>
      <trkpt lat="21.07250" lon="105.36380"><ele>690</ele></trkpt>
      <trkpt lat="21.07290" lon="105.36415"><ele>748</ele></trkpt>
      <trkpt lat="21.07335" lon="105.36440"><ele>812</ele></trkpt>
      <trkpt lat="21.07375" lon="105.36465"><ele>885</ele></trkpt>
      <trkpt lat="21.07410" lon="105.36490"><ele>960</ele></trkpt>
      <trkpt lat="21.07445" lon="105.36505"><ele>1042</ele></trkpt>
      <trkpt lat="21.07480" lon="105.36520"><ele>1126</ele></trkpt>
    </trkseg>
  </trk>
</gpx>`;

export interface DemoMemberTrack {
  callsign: string;
  color: string;
  csv: string;
}

/**
 * Generate a realistic 3-member synchronized expedition on Nhìu Cồ San (2965m).
 */
export function getDemoTeamReplayTracks(): DemoMemberTrack[] {
  const baseTs = 1763340000; // 2025-11-17 UTC morning
  const waypoints = [
    { lat: 22.6180, lon: 103.5710, alt: 1450 },
    { lat: 22.6205, lon: 103.5735, alt: 1620 },
    { lat: 22.6232, lon: 103.5762, alt: 1850 }, // Thac Ong Chua
    { lat: 22.6260, lon: 103.5788, alt: 2140 },
    { lat: 22.6285, lon: 103.5815, alt: 2400 }, // Lan nghi 2400m
    { lat: 22.6310, lon: 103.5842, alt: 2680 },
    { lat: 22.6338, lon: 103.5870, alt: 2965 }, // Dinh Nhiu Co San
  ];

  const makeCsv = (paceMultiplier: number, offsetLat: number) => {
    const rows = ['ts,lat,lon,alt'];
    const totalSteps = 60;
    for (let i = 0; i <= totalSteps; i++) {
      const progress = Math.min(1, (i / totalSteps) * paceMultiplier);
      const exactIdx = progress * (waypoints.length - 1);
      const idx = Math.min(waypoints.length - 2, Math.floor(exactIdx));
      const frac = exactIdx - idx;
      const p1 = waypoints[idx];
      const p2 = waypoints[idx + 1];
      const lat = p1.lat + (p2.lat - p1.lat) * frac + offsetLat;
      const lon = p1.lon + (p2.lon - p1.lon) * frac;
      const alt = p1.alt + (p2.alt - p1.alt) * frac;
      rows.push(`${baseTs + i * 180},${lat.toFixed(6)},${lon.toFixed(6)},${alt.toFixed(1)}`);
    }
    return rows.join('\n');
  };

  return [
    { callsign: 'LEADER', color: '#f59e0b', csv: makeCsv(1.05, 0.0) },
    { callsign: 'DIEM', color: '#10b981', csv: makeCsv(1.0, 0.00018) },
    { callsign: 'SWEEP', color: '#38bdf8', csv: makeCsv(0.9, -0.00018) },
  ];
}

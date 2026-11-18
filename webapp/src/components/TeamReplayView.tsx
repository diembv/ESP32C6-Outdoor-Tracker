import { useEffect, useMemo, useRef, useState } from 'preact/hooks';
import maplibregl from 'maplibre-gl';
import { computeTrackStats, parseCompactCsv, type TrackPoint, type TrackStats } from '../core/csv_to_gpx';
import { parseGpx } from '../core/rdp_compiler';
import { getDemoTeamReplayTracks } from '../core/demo_data';
import { registerOfflineTopoProtocol } from './TopoMapView';

interface MemberTrack {
  id: string;
  callsign: string;
  color: string;
  points: TrackPoint[];
  stats: TrackStats;
}

const COLORS = ['#f59e0b', '#10b981', '#38bdf8', '#f472b6', '#a3e635', '#fb7185'];

function formatDur(s: number): string {
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m ${s % 60}s`;
}

export function TeamReplayView({ onNotify }: { onNotify: (msg: string) => void }) {
  const [tracks, setTracks] = useState<MemberTrack[]>([]);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(20);
  const [currentTs, setCurrentTs] = useState(0);

  const mapBoxRef = useRef<HTMLDivElement | null>(null);
  const mapRef = useRef<maplibregl.Map | null>(null);
  const markersRef = useRef<Map<string, maplibregl.Marker>>(new Map());
  const fileRef = useRef<HTMLInputElement | null>(null);

  function loadDemoExpedition() {
    const demo = getDemoTeamReplayTracks();
    const parsed: MemberTrack[] = demo.map((d, idx) => {
      const points = parseCompactCsv(d.csv);
      return {
        id: `demo-${idx}`,
        callsign: d.callsign,
        color: d.color,
        points,
        stats: computeTrackStats(points),
      };
    });
    setTracks(parsed);
    setPlaying(false);
    if (parsed[0]?.points[0]) setCurrentTs(parsed[0].points[0].ts);
    onNotify('Đã nạp dữ liệu mô phỏng đoàn Nhìu Cồ San (2965m)');
  }

  useEffect(() => {
    loadDemoExpedition();
  }, []);

  const timeBounds = useMemo(() => {
    if (tracks.length === 0) return { min: 0, max: 0 };
    let min = Infinity;
    let max = -Infinity;
    for (const t of tracks) {
      if (t.points.length === 0) continue;
      if (t.points[0].ts < min) min = t.points[0].ts;
      const last = t.points[t.points.length - 1].ts;
      if (last > max) max = last;
    }
    return min === Infinity ? { min: 0, max: 0 } : { min, max };
  }, [tracks]);

  // Initialize MapLibre
  useEffect(() => {
    if (!mapBoxRef.current) return;
    registerOfflineTopoProtocol();

    const map = new maplibregl.Map({
      container: mapBoxRef.current,
      center: [103.579, 22.626],
      zoom: 13.5,
      style: {
        version: 8,
        sources: {
          topo: {
            type: 'raster',
            tiles: ['offline-topo://{z}/{x}/{y}'],
            tileSize: 256,
            attribution: '© OpenTopoMap',
          },
        },
        layers: [{ id: 'topo-layer', type: 'raster', source: 'topo' }],
      },
    });
    mapRef.current = map;
    return () => {
      markersRef.current.forEach((m) => m.remove());
      markersRef.current.clear();
      map.remove();
      mapRef.current = null;
    };
  }, []);

  // Draw tracks on map when tracks change
  useEffect(() => {
    const map = mapRef.current;
    if (!map || tracks.length === 0) return;

    const applyTracks = () => {
      // Remove old layers/sources
      for (let i = 0; i < 10; i++) {
        if (map.getLayer(`replay-line-${i}`)) map.removeLayer(`replay-line-${i}`);
        if (map.getSource(`replay-src-${i}`)) map.removeSource(`replay-src-${i}`);
      }
      markersRef.current.forEach((m) => m.remove());
      markersRef.current.clear();

      const bounds = new maplibregl.LngLatBounds();
      tracks.forEach((t, idx) => {
        if (t.points.length === 0) return;
        t.points.forEach((p) => bounds.extend([p.lon, p.lat]));

        map.addSource(`replay-src-${idx}`, {
          type: 'geojson',
          data: {
            type: 'Feature',
            geometry: {
              type: 'LineString',
              coordinates: t.points.map((p) => [p.lon, p.lat]),
            },
            properties: {},
          },
        });
        map.addLayer({
          id: `replay-line-${idx}`,
          type: 'line',
          source: `replay-src-${idx}`,
          paint: {
            'line-color': t.color,
            'line-width': 3,
            'line-opacity': 0.75,
          },
        });

        const el = document.createElement('div');
        el.style.cssText = `background:${t.color};color:#0b1015;padding:2px 7px;border-radius:999px;font-family:JetBrains Mono,monospace;font-size:11px;font-weight:800;border:2px solid #fff;box-shadow:0 2px 8px rgba(0,0,0,0.6);`;
        el.textContent = t.callsign;
        const marker = new maplibregl.Marker({ element: el })
          .setLngLat([t.points[0].lon, t.points[0].lat])
          .addTo(map);
        markersRef.current.set(t.id, marker);
      });

      if (!bounds.isEmpty()) {
        map.fitBounds(bounds, { padding: 40, duration: 500 });
      }
    };

    if (map.isStyleLoaded()) applyTracks();
    else map.once('load', applyTracks);
  }, [tracks]);

  // Playback loop
  useEffect(() => {
    if (!playing || timeBounds.max <= timeBounds.min) return;
    const timer = setInterval(() => {
      setCurrentTs((prev) => {
        const next = prev + speed * 2;
        if (next >= timeBounds.max) {
          setPlaying(false);
          return timeBounds.max;
        }
        return next;
      });
    }, 100);
    return () => clearInterval(timer);
  }, [playing, speed, timeBounds]);

  // Interpolate member positions at currentTs
  useEffect(() => {
    tracks.forEach((t) => {
      const marker = markersRef.current.get(t.id);
      if (!marker || t.points.length === 0) return;

      const pts = t.points;
      if (currentTs <= pts[0].ts) {
        marker.setLngLat([pts[0].lon, pts[0].lat]);
        return;
      }
      if (currentTs >= pts[pts.length - 1].ts) {
        const last = pts[pts.length - 1];
        marker.setLngLat([last.lon, last.lat]);
        return;
      }
      for (let i = 1; i < pts.length; i++) {
        if (pts[i].ts >= currentTs) {
          const p1 = pts[i - 1];
          const p2 = pts[i];
          const frac = (currentTs - p1.ts) / Math.max(1, p2.ts - p1.ts);
          marker.setLngLat([
            p1.lon + (p2.lon - p1.lon) * frac,
            p1.lat + (p2.lat - p1.lat) * frac,
          ]);
          break;
        }
      }
    });
  }, [currentTs, tracks]);

  function handleUploadFiles(files: FileList) {
    Array.from(files).forEach((file, idx) => {
      const reader = new FileReader();
      reader.onload = () => {
        if (typeof reader.result !== 'string') return;
        let pts: TrackPoint[] = [];
        if (file.name.toLowerCase().endsWith('.csv')) {
          pts = parseCompactCsv(reader.result);
        } else {
          const g = parseGpx(reader.result);
          pts = g.points
            .filter((p) => p.ts !== undefined)
            .map((p) => ({ lat: p.lat, lon: p.lon, ele: p.ele ?? 0, ts: p.ts! }));
        }
        if (pts.length === 0) {
          onNotify(`${file.name} không có dữ liệu thời gian UTC hợp lệ`);
          return;
        }
        const callsign = file.name.replace(/\.(csv|gpx)$/i, '').slice(0, 8).toUpperCase();
        setTracks((prev) => [
          ...prev,
          {
            id: `${file.name}-${Date.now()}-${idx}`,
            callsign,
            color: COLORS[(prev.length + idx) % COLORS.length],
            points: pts,
            stats: computeTrackStats(pts),
          },
        ]);
      };
      reader.readAsText(file);
    });
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div class="panel">
        <div class="panel-header">
          <div>
            <h2 class="panel-title">Phát Lại Hành Trình Cả Đoàn (Synchronized UTC Replay)</h2>
            <p class="panel-subtitle">
              Đồng bộ hóa nhiều file .CSV / .GPX theo mốc thời gian vệ tinh UTC Epoch
            </p>
          </div>
          <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
            <input
              ref={fileRef}
              type="file"
              multiple
              accept=".csv,.gpx"
              style={{ display: 'none' }}
              onChange={(e) => {
                const fl = (e.target as HTMLInputElement).files;
                if (fl) handleUploadFiles(fl);
              }}
            />
            <button class="btn btn-sm" onClick={() => fileRef.current?.click()}>
              + Thêm Track (.CSV / .GPX)
            </button>
            <button class="btn btn-sm" onClick={loadDemoExpedition}>
              Mẫu Nhìu Cồ San
            </button>
          </div>
        </div>

        <div ref={mapBoxRef} class="map-box" style={{ height: '420px', marginBottom: '14px' }} />

        {/* Playback Controls */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
          <button
            class="btn btn-primary btn-sm"
            style={{ minWidth: '90px' }}
            onClick={() => {
              if (currentTs >= timeBounds.max) setCurrentTs(timeBounds.min);
              setPlaying((p) => !p);
            }}
          >
            {playing ? 'Tạm dừng' : 'Phát Replay'}
          </button>

          <select
            class="select"
            style={{ width: '90px' }}
            value={speed}
            onChange={(e) => setSpeed(Number((e.target as HTMLSelectElement).value))}
          >
            <option value={5}>5x</option>
            <option value={20}>20x</option>
            <option value={60}>60x</option>
            <option value={180}>180x</option>
          </select>

          <input
            type="range"
            min={timeBounds.min}
            max={Math.max(timeBounds.min + 1, timeBounds.max)}
            value={currentTs}
            style={{ flex: 1 }}
            onInput={(e) => setCurrentTs(Number((e.target as HTMLInputElement).value))}
          />

          <span class="mono" style={{ fontSize: '12px', color: 'var(--accent-amber)' }}>
            {currentTs > 0 ? new Date(currentTs * 1000).toISOString().slice(11, 19) + ' UTC' : '--:--:--'}
          </span>
        </div>
      </div>

      {/* Analytics Table */}
      <div class="panel">
        <div class="panel-header">
          <h2 class="panel-title">Thống Kê Leo Núi Chuyên Sâu</h2>
        </div>
        <table class="list-table">
          <thead>
            <tr>
              <th>Thành viên</th>
              <th>Quãng đường</th>
              <th>Di chuyển / Nghỉ</th>
              <th>Leo lên (+Gain)</th>
              <th>Đỉnh cao nhất</th>
              <th>VAM (m/giờ)</th>
            </tr>
          </thead>
          <tbody>
            {tracks.map((t) => (
              <tr key={t.id}>
                <td class="mono" style={{ fontWeight: 700, color: t.color }}>
                  ● {t.callsign}
                </td>
                <td class="mono">{t.stats.totalDistKm.toFixed(2)} km</td>
                <td class="mono">
                  {formatDur(t.stats.movingTimeS)} / {formatDur(t.stats.stoppedTimeS)}
                </td>
                <td class="mono" style={{ color: 'var(--accent-emerald)' }}>
                  +{t.stats.elevationGainM} m
                </td>
                <td class="mono">{t.stats.maxAltM} m</td>
                <td class="mono" style={{ color: 'var(--accent-amber)', fontWeight: 600 }}>
                  {t.stats.avgVamMh} m/h
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

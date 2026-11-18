import { useEffect, useMemo, useRef, useState } from 'preact/hooks';
import {
  compileRouteBin,
  haversineDistanceM,
  parseGpx,
  ramerDouglasPeucker,
} from '../core/rdp_compiler';
import { SAMPLE_BAVI_GPX } from '../core/demo_data';
import {
  clearTileCache,
  downloadTilesForRoute,
  getTileCacheStats,
  getTilesForBounds,
  type TileCacheStats,
} from '../core/offline_tiles';
import {
  WAYPOINT_ICONS,
  type CompiledRoute,
  type GeoPoint,
  type RouteWaypoint,
  type WaypointIcon,
} from '../core/types';
import type { DeviceClient } from '../core/device_client';

interface Props {
  client: DeviceClient;
  onCompiledRoute: (route: CompiledRoute) => void;
  onNotify: (msg: string) => void;
}

export function RouteStudio({ client, onCompiledRoute, onNotify }: Props) {
  const [routeName, setRouteName] = useState('Ba Vi Summit');
  const [rawPoints, setRawPoints] = useState<GeoPoint[]>([]);
  const [waypoints, setWaypoints] = useState<RouteWaypoint[]>([]);
  const [epsilon, setEpsilon] = useState(5);
  const [dragging, setDragging] = useState(false);
  const [uploading, setUploading] = useState(false);

  // Waypoint draft
  const [wptLabel, setWptLabel] = useState('LAN NGHI');
  const [wptIcon, setWptIcon] = useState<WaypointIcon>(2);

  // Offline tile caching
  const [tileStats, setTileStats] = useState<TileCacheStats>({ tileCount: 0, estimatedMb: 0 });
  const [tileProgress, setTileProgress] = useState<{ done: number; total: number } | null>(null);
  const abortRef = useRef<AbortController | null>(null);
  const fileInputRef = useRef<HTMLInputElement | null>(null);

  useEffect(() => {
    loadGpxText(SAMPLE_BAVI_GPX);
    refreshTileStats();
  }, []);

  async function refreshTileStats() {
    setTileStats(await getTileCacheStats());
  }

  function loadGpxText(xml: string) {
    const parsed = parseGpx(xml);
    if (parsed.points.length === 0) {
      onNotify('Không tìm thấy điểm tọa độ (trkpt/rtept) trong file GPX');
      return;
    }
    setRouteName(parsed.name.slice(0, 31));
    setRawPoints(parsed.points);
    setWaypoints(parsed.waypoints);
  }

  function handleFile(file: File) {
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result === 'string') {
        loadGpxText(reader.result);
        onNotify(`Đã nạp ${file.name}`);
      }
    };
    reader.readAsText(file);
  }

  const compiled = useMemo(() => {
    if (rawPoints.length === 0) return null;
    const c = compileRouteBin(routeName, rawPoints, waypoints, epsilon);
    onCompiledRoute(c);
    return c;
  }, [rawPoints, routeName, waypoints, epsilon]);

  const compressedPoints = useMemo(
    () => (rawPoints.length ? ramerDouglasPeucker(rawPoints, epsilon) : []),
    [rawPoints, epsilon]
  );

  // Elevation profile SVG geometry
  const profileData = useMemo(() => {
    if (rawPoints.length < 2) return null;
    let cumDistKm = 0;
    const items = rawPoints.map((p, i) => {
      if (i > 0) {
        cumDistKm += haversineDistanceM(rawPoints[i - 1].lat, rawPoints[i - 1].lon, p.lat, p.lon) / 1000;
      }
      return {
        lat: p.lat,
        lon: p.lon,
        ele: p.ele ?? 0,
        distKm: cumDistKm,
      };
    });
    const minEle = Math.min(...items.map((d) => d.ele));
    const maxEle = Math.max(...items.map((d) => d.ele));
    const totalKm = Math.max(0.01, cumDistKm);
    return { items, minEle, maxEle, totalKm };
  }, [rawPoints]);

  function handleProfileClick(e: MouseEvent) {
    if (!profileData || profileData.items.length === 0) return;
    const svg = e.currentTarget as SVGSVGElement;
    const rect = svg.getBoundingClientRect();
    const ratio = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
    const idx = Math.min(
      profileData.items.length - 1,
      Math.round(ratio * (profileData.items.length - 1))
    );
    const pt = profileData.items[idx];
    const cleanLbl = wptLabel.trim().slice(0, 20) || `WPT ${waypoints.length + 1}`;
    setWaypoints((prev) => [
      ...prev,
      {
        lat: pt.lat,
        lon: pt.lon,
        alt_m: Math.round(pt.ele),
        icon_type: wptIcon,
        label: cleanLbl.toUpperCase(),
      },
    ]);
    onNotify(`Đã gắn mốc "${cleanLbl.toUpperCase()}" tại ${Math.round(pt.ele)}m`);
  }

  async function handleUploadToDevice() {
    if (!compiled) return;
    setUploading(true);
    try {
      const filename = `${compiled.header.name.toLowerCase().replace(/[^a-z0-9]+/g, '_') || 'active'}.bin`;
      await client.uploadRoute(filename, compiled.buffer);
      onNotify(`Đã nạp ${filename} (${compiled.buffer.byteLength} bytes) lên ESP32!`);
    } catch (err) {
      onNotify(`Lỗi nạp route: ${(err as Error).message}`);
    } finally {
      setUploading(false);
    }
  }

  function handleDownloadBin() {
    if (!compiled) return;
    const blob = new Blob([compiled.buffer.buffer as ArrayBuffer], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${compiled.header.name.toLowerCase().replace(/[^a-z0-9]+/g, '_') || 'route'}.bin`;
    a.click();
    URL.revokeObjectURL(url);
  }

  async function handleDownloadOfflineTiles() {
    if (!compiled) return;
    const { min_lat, max_lat, min_lon, max_lon } = compiled.header;
    const tiles = getTilesForBounds(min_lat, max_lat, min_lon, max_lon, 12, 15, 2.0);
    if (tiles.length > 600) {
      onNotify(`Vùng quá rộng (${tiles.length} ô). Giới hạn tối đa 600 ô bản đồ.`);
      return;
    }
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setTileProgress({ done: 0, total: tiles.length });
    const res = await downloadTilesForRoute(
      tiles,
      (done, total) => setTileProgress({ done, total }),
      ctrl.signal
    );
    setTileProgress(null);
    await refreshTileStats();
    onNotify(`Đã tải ${res.downloaded} ô bản đồ mới (bỏ qua ${res.skipped} ô đã có)`);
  }

  const savedPct =
    rawPoints.length > 0
      ? Math.max(0, Math.round((1 - compressedPoints.length / rawPoints.length) * 100))
      : 0;

  return (
    <div class="grid-sidebar">
      {/* Left Column: GPX Input, RDP Settings, Offline Tiles */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <div class="panel">
          <div class="panel-header">
            <div>
              <h2 class="panel-title">1. Nạp & Nén Lộ Trình GPX</h2>
              <p class="panel-subtitle">Ramer-Douglas-Peucker TypeScript → Binary v2</p>
            </div>
            <button
              class="btn btn-sm"
              onClick={() => {
                loadGpxText(SAMPLE_BAVI_GPX);
                onNotify('Đã nạp cung đường mẫu Ba Vì Summit');
              }}
            >
              Mẫu Ba Vì
            </button>
          </div>

          <div
            class={`dropzone ${dragging ? 'drag' : ''}`}
            onClick={() => fileInputRef.current?.click()}
            onDragOver={(e) => {
              e.preventDefault();
              setDragging(true);
            }}
            onDragLeave={() => setDragging(false)}
            onDrop={(e) => {
              e.preventDefault();
              setDragging(false);
              const f = e.dataTransfer?.files?.[0];
              if (f) handleFile(f);
            }}
          >
            <input
              ref={fileInputRef}
              type="file"
              accept=".gpx,application/gpx+xml"
              style={{ display: 'none' }}
              onChange={(e) => {
                const f = (e.target as HTMLInputElement).files?.[0];
                if (f) handleFile(f);
              }}
            />
            <div style={{ fontWeight: 600, marginBottom: '4px' }}>Kéo thả file .GPX hoặc bấm để chọn</div>
            <div style={{ fontSize: '12px', color: 'var(--text-secondary)' }}>
              Hỗ trợ UpTrail, Wikiloc, AllTrails, Garmin
            </div>
          </div>

          <div class="field" style={{ marginTop: '14px' }}>
            <label class="field-label">Tên lộ trình (ASCII tối đa 31 ký tự)</label>
            <input
              class="input"
              value={routeName}
              maxLength={31}
              onInput={(e) => setRouteName((e.target as HTMLInputElement).value)}
            />
          </div>

          <div class="field">
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
              <label class="field-label" style={{ margin: 0 }}>Ngưỡng nén RDP (Epsilon)</label>
              <span class="mono" style={{ fontSize: '12px', color: 'var(--accent-amber)' }}>
                {epsilon} m
              </span>
            </div>
            <input
              type="range"
              min={1}
              max={25}
              step={1}
              value={epsilon}
              style={{ width: '100%' }}
              onInput={(e) => setEpsilon(Number((e.target as HTMLInputElement).value))}
            />
          </div>

          {compiled && (
            <div class="stat-grid" style={{ marginBottom: '14px' }}>
              <div class="stat-box">
                <div class="stat-label">Quãng đường</div>
                <div class="stat-val">{compiled.header.total_dist_km.toFixed(2)} km</div>
              </div>
              <div class="stat-box">
                <div class="stat-label">Số điểm sau RDP</div>
                <div class="stat-val">
                  {compiled.header.point_count}
                  <span style={{ fontSize: '11px', color: 'var(--accent-emerald)', marginLeft: '4px' }}>
                    (-{savedPct}%)
                  </span>
                </div>
              </div>
              <div class="stat-box">
                <div class="stat-label">Kích thước .BIN</div>
                <div class="stat-val">{compiled.buffer.byteLength} B</div>
              </div>
            </div>
          )}

          <div style={{ display: 'flex', gap: '8px' }}>
            <button
              class="btn btn-primary"
              style={{ flex: 1 }}
              disabled={!compiled || uploading}
              onClick={handleUploadToDevice}
            >
              {uploading ? 'Đang nạp...' : 'Nạp lên ESP32'}
            </button>
            <button class="btn" disabled={!compiled} onClick={handleDownloadBin}>
              Tải .BIN
            </button>
          </div>
        </div>

        {/* Offline Topo Tile Caching */}
        <div class="panel">
          <div class="panel-header">
            <div>
              <h2 class="panel-title">2. Bản Đồ Địa Hình Offline</h2>
              <p class="panel-subtitle">OpenTopoMap (Zoom 12–15 + Đệm 2km)</p>
            </div>
            <span class="pill mono">{tileStats.tileCount} ô (~{tileStats.estimatedMb} MB)</span>
          </div>

          {tileProgress ? (
            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
                <span>Đang tải ô bản đồ...</span>
                <span class="mono">
                  {tileProgress.done} / {tileProgress.total}
                </span>
              </div>
              <div class="progress-bar">
                <div
                  class="progress-fill"
                  style={{ width: `${Math.round((tileProgress.done / tileProgress.total) * 100)}%` }}
                />
              </div>
              <button
                class="btn btn-sm btn-danger"
                style={{ marginTop: '10px', width: '100%' }}
                onClick={() => abortRef.current?.abort()}
              >
                Hủy tải
              </button>
            </div>
          ) : (
            <div style={{ display: 'flex', gap: '8px' }}>
              <button
                class="btn btn-emerald"
                style={{ flex: 1 }}
                disabled={!compiled}
                onClick={handleDownloadOfflineTiles}
              >
                Tải Topo Offline cho tuyến này
              </button>
              <button
                class="btn btn-sm"
                onClick={async () => {
                  await clearTileCache();
                  await refreshTileStats();
                  onNotify('Đã xóa bộ nhớ đệm bản đồ offline');
                }}
              >
                Xóa Cache
              </button>
            </div>
          )}
        </div>
      </div>

      {/* Right Column: Elevation Profile + Waypoints Editor */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <div class="panel">
          <div class="panel-header">
            <div>
              <h2 class="panel-title">Biểu Đồ Độ Cao & Gắn Mốc Waypoints (Click lên biểu đồ)</h2>
              <p class="panel-subtitle">
                Hiển thị độ cao (m) và độ dốc cung đường — Hỗ trợ chuẩn RouteHeader v2
              </p>
            </div>
            {profileData && (
              <span class="pill mono">
                {Math.round(profileData.minEle)}m → {Math.round(profileData.maxEle)}m
              </span>
            )}
          </div>

          {/* Waypoint Quick Controls */}
          <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '12px' }}>
            <select
              class="select"
              style={{ width: '170px' }}
              value={wptIcon}
              onChange={(e) => setWptIcon(Number((e.target as HTMLSelectElement).value) as WaypointIcon)}
            >
              <option value={1}>Đỉnh núi (SUMMIT)</option>
              <option value={2}>Lán nghỉ (CAMP)</option>
              <option value={3}>Suối / Nước (WATER)</option>
              <option value={4}>Nguy hiểm (DANGER)</option>
              <option value={0}>Mốc chung (WPT)</option>
            </select>
            <input
              class="input"
              style={{ flex: 1, minWidth: '160px' }}
              placeholder="Tên điểm mốc (VD: THAC ONG CHUA)"
              maxLength={20}
              value={wptLabel}
              onInput={(e) => setWptLabel((e.target as HTMLInputElement).value)}
            />
          </div>

          {/* Interactive SVG Elevation Profile */}
          {profileData ? (
            <div
              style={{
                background: 'var(--bg-base)',
                borderRadius: 'var(--radius-md)',
                border: '1px solid var(--border-subtle)',
                padding: '12px',
              }}
            >
              <svg
                viewBox="0 0 800 210"
                style={{ width: '100%', height: '210px', cursor: 'crosshair', display: 'block' }}
                onClick={handleProfileClick}
              >
                <defs>
                  <linearGradient id="eleFill" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor="#f59e0b" stopOpacity="0.4" />
                    <stop offset="100%" stopColor="#f59e0b" stopOpacity="0.03" />
                  </linearGradient>
                </defs>

                {/* Grid lines */}
                {[0.25, 0.5, 0.75].map((f) => (
                  <line
                    key={f}
                    x1="0"
                    y1={190 * f}
                    x2="800"
                    y2={190 * f}
                    stroke="rgba(148,163,184,0.12)"
                    strokeDasharray="4 4"
                  />
                ))}

                {/* Area + Path */}
                {(() => {
                  const { items, minEle, maxEle, totalKm } = profileData;
                  const spanEle = Math.max(20, maxEle - minEle);
                  const pts = items.map((d) => {
                    const x = (d.distKm / totalKm) * 800;
                    const y = 185 - ((d.ele - minEle) / spanEle) * 160;
                    return `${x.toFixed(1)},${y.toFixed(1)}`;
                  });
                  const areaD = `M 0,190 L ${pts.join(' L ')} L 800,190 Z`;
                  const lineD = `M ${pts.join(' L ')}`;
                  return (
                    <>
                      <path d={areaD} fill="url(#eleFill)" />
                      <path d={lineD} fill="none" stroke="#f59e0b" strokeWidth="2.5" />
                    </>
                  );
                })()}

                {/* Waypoint Markers */}
                {waypoints.map((w, idx) => {
                  // Match nearest rawPoint
                  let bestIdx = 0;
                  let bestD = Infinity;
                  profileData.items.forEach((p, i) => {
                    const d = Math.hypot(p.lat - w.lat, p.lon - w.lon);
                    if (d < bestD) {
                      bestD = d;
                      bestIdx = i;
                    }
                  });
                  const item = profileData.items[bestIdx];
                  const x = (item.distKm / profileData.totalKm) * 800;
                  const spanEle = Math.max(20, profileData.maxEle - profileData.minEle);
                  const y = 185 - ((item.ele - profileData.minEle) / spanEle) * 160;
                  const meta = WAYPOINT_ICONS[w.icon_type] || WAYPOINT_ICONS[0];
                  return (
                    <g key={idx}>
                      <line x1={x} y1={y} x2={x} y2={190} stroke={meta.color} strokeDasharray="2 2" />
                      <circle cx={x} cy={y} r={5} fill={meta.color} stroke="#0b1015" strokeWidth="1.5" />
                      <text
                        x={Math.min(730, Math.max(6, x + 6))}
                        y={Math.max(14, y - 8)}
                        fill={meta.color}
                        fontSize="10"
                        fontFamily="JetBrains Mono, monospace"
                        fontWeight="700"
                      >
                        {w.label}
                      </text>
                    </g>
                  );
                })}
              </svg>
            </div>
          ) : null}
        </div>

        {/* Waypoint Table */}
        <div class="panel">
          <div class="panel-header">
            <h2 class="panel-title">Danh Sách Điểm Mốc Waypoints ({waypoints.length})</h2>
          </div>
          {waypoints.length === 0 ? (
            <div style={{ color: 'var(--text-muted)', fontSize: '13px' }}>
              Chưa có điểm mốc nào. Bấm vào biểu đồ độ cao phía trên để gắn mốc mới.
            </div>
          ) : (
            <table class="list-table">
              <thead>
                <tr>
                  <th>Loại</th>
                  <th>Nhãn (20B)</th>
                  <th>Độ cao</th>
                  <th>Tọa độ</th>
                  <th></th>
                </tr>
              </thead>
              <tbody>
                {waypoints.map((w, idx) => {
                  const meta = WAYPOINT_ICONS[w.icon_type] || WAYPOINT_ICONS[0];
                  return (
                    <tr key={idx}>
                      <td>
                        <span
                          class="pill mono"
                          style={{ borderColor: meta.color, color: meta.color, fontSize: '11px' }}
                        >
                          {meta.badge}
                        </span>
                      </td>
                      <td class="mono" style={{ fontWeight: 600 }}>
                        {w.label}
                      </td>
                      <td class="mono">{w.alt_m} m</td>
                      <td class="mono" style={{ color: 'var(--text-secondary)', fontSize: '12px' }}>
                        {w.lat.toFixed(5)}, {w.lon.toFixed(5)}
                      </td>
                      <td style={{ textAlign: 'right' }}>
                        <button
                          class="btn btn-sm btn-danger"
                          onClick={() => setWaypoints((prev) => prev.filter((_, i) => i !== idx))}
                        >
                          Xóa
                        </button>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          )}
        </div>
      </div>
    </div>
  );
}

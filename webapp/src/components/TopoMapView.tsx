import { useEffect, useRef } from 'preact/hooks';
import maplibregl from 'maplibre-gl';
import 'maplibre-gl/dist/maplibre-gl.css';
import { fetchOfflineFirstTile } from '../core/offline_tiles';
import { WAYPOINT_ICONS, type CompiledRoute, type DeviceStatus, type PeerInfo } from '../core/types';
import type { DeviceClient } from '../core/device_client';

interface Props {
  client: DeviceClient;
  status: DeviceStatus | null;
  peers: PeerInfo[];
  activeRoute: CompiledRoute | null;
  onRefresh: () => void;
  onNotify: (msg: string) => void;
}

let protocolRegistered = false;

export function registerOfflineTopoProtocol() {
  if (protocolRegistered) return;
  protocolRegistered = true;
  maplibregl.addProtocol('offline-topo', async (params) => {
    const match = params.url.match(/offline-topo:\/\/(\d+)\/(\d+)\/(\d+)/);
    if (!match) throw new Error('Invalid offline-topo URL');
    const [, z, x, y] = match;
    const data = await fetchOfflineFirstTile(Number(z), Number(x), Number(y));
    return { data };
  });
}

export function TopoMapView({ client, status, peers, activeRoute, onRefresh, onNotify }: Props) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const mapRef = useRef<maplibregl.Map | null>(null);
  const selfMarkerRef = useRef<maplibregl.Marker | null>(null);
  const peerMarkersRef = useRef<Map<number, maplibregl.Marker>>(new Map());
  const wptMarkersRef = useRef<maplibregl.Marker[]>([]);

  useEffect(() => {
    if (!containerRef.current) return;
    registerOfflineTopoProtocol();

    const centerLon = status?.lon || 105.3635;
    const centerLat = status?.lat || 21.0725;

    const map = new maplibregl.Map({
      container: containerRef.current,
      center: [centerLon, centerLat],
      zoom: 14,
      style: {
        version: 8,
        sources: {
          topo: {
            type: 'raster',
            tiles: ['offline-topo://{z}/{x}/{y}'],
            tileSize: 256,
            attribution: '© OpenTopoMap (Offline IndexedDB Ready)',
          },
        },
        layers: [
          {
            id: 'topo-layer',
            type: 'raster',
            source: 'topo',
          },
        ],
      },
    });

    map.addControl(new maplibregl.NavigationControl(), 'top-right');

    map.on('load', () => {
      map.addSource('active-route', {
        type: 'geojson',
        data: {
          type: 'Feature',
          geometry: { type: 'LineString', coordinates: [] },
          properties: {},
        },
      });
      map.addLayer({
        id: 'active-route-line',
        type: 'line',
        source: 'active-route',
        paint: {
          'line-color': '#f59e0b',
          'line-width': 4,
          'line-opacity': 0.9,
        },
      });
    });

    mapRef.current = map;
    return () => {
      peerMarkersRef.current.forEach((m) => m.remove());
      peerMarkersRef.current.clear();
      selfMarkerRef.current?.remove();
      map.remove();
      mapRef.current = null;
    };
  }, []);

  // Update Active Route polyline & Waypoints
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !activeRoute) return;

    const updateRoute = () => {
      const src = map.getSource('active-route') as maplibregl.GeoJSONSource | undefined;
      if (src) {
        src.setData({
          type: 'Feature',
          geometry: {
            type: 'LineString',
            coordinates: activeRoute.points.map((p) => [p.lon, p.lat]),
          },
          properties: {},
        });
      }

      wptMarkersRef.current.forEach((m) => m.remove());
      wptMarkersRef.current = activeRoute.waypoints.map((w) => {
        const meta = WAYPOINT_ICONS[w.icon_type] || WAYPOINT_ICONS[0];
        const el = document.createElement('div');
        el.style.cssText = `background:#0b1015;color:${meta.color};border:1.5px solid ${meta.color};padding:2px 6px;border-radius:4px;font-family:JetBrains Mono,monospace;font-size:10px;font-weight:700;white-space:nowrap;`;
        el.textContent = `${w.label} (${w.alt_m}m)`;
        return new maplibregl.Marker({ element: el }).setLngLat([w.lon, w.lat]).addTo(map);
      });
    };

    if (map.isStyleLoaded()) updateRoute();
    else map.once('load', updateRoute);
  }, [activeRoute]);

  // Update Self & Team Peer Markers
  useEffect(() => {
    const map = mapRef.current;
    if (!map) return;

    if (status && status.gps_fix) {
      if (!selfMarkerRef.current) {
        const el = document.createElement('div');
        el.style.cssText =
          'width:16px;height:16px;border-radius:50%;background:#10b981;border:3px solid #fff;box-shadow:0 0 12px rgba(16,185,129,0.9);';
        selfMarkerRef.current = new maplibregl.Marker({ element: el })
          .setLngLat([status.lon, status.lat])
          .addTo(map);
      } else {
        selfMarkerRef.current.setLngLat([status.lon, status.lat]);
      }
    }

    const activeIds = new Set(peers.map((p) => p.node_id));
    for (const [id, marker] of peerMarkersRef.current.entries()) {
      if (!activeIds.has(id)) {
        marker.remove();
        peerMarkersRef.current.delete(id);
      }
    }

    peers.forEach((p) => {
      let marker = peerMarkersRef.current.get(p.node_id);
      if (!marker) {
        const el = document.createElement('div');
        el.style.cssText = `background:${p.sos ? '#ef4444' : '#0f172a'};color:${
          p.sos ? '#fff' : '#38bdf8'
        };border:1.5px solid ${p.sos ? '#fca5a5' : '#38bdf8'};padding:2px 7px;border-radius:999px;font-family:JetBrains Mono,monospace;font-size:11px;font-weight:700;box-shadow:0 2px 8px rgba(0,0,0,0.5);`;
        el.textContent = p.sos ? `SOS ${p.callsign}` : p.callsign;
        marker = new maplibregl.Marker({ element: el }).setLngLat([p.lon, p.lat]).addTo(map);
        peerMarkersRef.current.set(p.node_id, marker);
      } else {
        marker.setLngLat([p.lon, p.lat]);
      }
    });
  }, [status, peers]);

  async function runCmd(label: string, fn: () => Promise<void>) {
    try {
      await fn();
      onNotify(label);
      onRefresh();
    } catch (e) {
      onNotify(`Lỗi: ${(e as Error).message}`);
    }
  }

  return (
    <div class="grid-sidebar">
      {/* Left: Tactical Telemetry & Team Peer Table */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <div class="panel">
          <div class="panel-header">
            <div>
              <h2 class="panel-title">Định Vị & Điều Khiển Từ Xa</h2>
              <p class="panel-subtitle">Cầu nối GPS + Khí áp kế BMP580</p>
            </div>
            {status && (
              <button
                class="btn btn-sm"
                onClick={() => mapRef.current?.flyTo({ center: [status.lon, status.lat], zoom: 15 })}
              >
                Về vị trí
              </button>
            )}
          </div>

          <div class="stat-grid" style={{ marginBottom: '12px' }}>
            <div class="stat-box">
              <div class="stat-label">Độ cao Baro</div>
              <div class="stat-val" style={{ color: 'var(--accent-amber)' }}>
                {status ? `${status.baro_alt_m.toFixed(1)}m` : '--'}
              </div>
            </div>
            <div class="stat-box">
              <div class="stat-label">Tốc độ</div>
              <div class="stat-val">{status ? `${status.speed_kmh.toFixed(1)} km/h` : '--'}</div>
            </div>
            <div class="stat-box">
              <div class="stat-label">Vệ tinh / HDOP</div>
              <div class="stat-val">{status ? `${status.satellites} / ${status.hdop}` : '--'}</div>
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <button
              class={`btn btn-sm ${status?.is_logging ? 'btn-emerald' : ''}`}
              onClick={() =>
                runCmd(status?.is_logging ? 'Đã tạm dừng ghi log CSV' : 'Đã bật ghi log CSV', () =>
                  client.sendCommand({ type: 'toggle_log' })
                )
              }
            >
              {status?.is_logging ? '● Đang ghi REC' : '○ Bật ghi REC'}
            </button>
            <button
              class="btn btn-sm"
              onClick={() =>
                runCmd('Đã hiệu chuẩn Baro theo GPS', () => client.sendCommand({ type: 'calib_baro' }))
              }
            >
              Calib Độ Cao
            </button>
            <button
              class="btn btn-sm"
              onClick={() => {
                const next = status?.activity_mode === 'HIKE' ? 'BIKE' : 'HIKE';
                runCmd(`Đã chuyển chế độ ${next}`, () =>
                  client.sendCommand({ type: 'set_mode', mode: next })
                );
              }}
            >
              Chế độ: {status?.activity_mode || 'HIKE'}
            </button>
            <button
              class={`btn btn-sm ${status?.sos_active ? 'btn-danger' : ''}`}
              onClick={() =>
                runCmd(status?.sos_active ? 'Đã tắt tín hiệu SOS' : 'ĐÃ PHÁT TÍN HIỆU SOS!', () =>
                  client.sendCommand({ type: 'toggle_sos', active: !status?.sos_active })
                )
              }
            >
              {status?.sos_active ? 'Hủy SOS' : 'Phát SOS'}
            </button>
          </div>
        </div>

        {/* Team Peers */}
        <div class="panel">
          <div class="panel-header">
            <div>
              <h2 class="panel-title">Đồng Đội Trong Đoàn ({peers.length})</h2>
              <p class="panel-subtitle">ESP-NOW Long-Range / LoRa Mesh</p>
            </div>
          </div>

          {peers.length === 0 ? (
            <div style={{ color: 'var(--text-muted)', fontSize: '13px' }}>
              Chưa nhận được gói tin TeamBeacon nào.
            </div>
          ) : (
            <table class="list-table">
              <thead>
                <tr>
                  <th>Callsign</th>
                  <th>Khoảng cách</th>
                  <th>Δh</th>
                  <th>Pin / Tuổi</th>
                </tr>
              </thead>
              <tbody>
                {peers.map((p) => (
                  <tr
                    key={p.node_id}
                    style={{ cursor: 'pointer' }}
                    onClick={() => mapRef.current?.flyTo({ center: [p.lon, p.lat], zoom: 16 })}
                  >
                    <td class="mono" style={{ fontWeight: 700, color: p.sos ? '#fca5a5' : 'var(--accent-sky)' }}>
                      #{p.node_id} {p.callsign}
                    </td>
                    <td class="mono">
                      {p.dist_m !== undefined ? `${p.dist_m}m (${p.bearing_deg}°)` : '--'}
                    </td>
                    <td
                      class="mono"
                      style={{
                        color:
                          (p.delta_alt_m ?? 0) >= 0 ? 'var(--accent-emerald)' : 'var(--accent-amber)',
                      }}
                    >
                      {p.delta_alt_m !== undefined
                        ? `${p.delta_alt_m >= 0 ? '+' : ''}${p.delta_alt_m}m`
                        : `${p.alt_m}m`}
                    </td>
                    <td class="mono" style={{ color: 'var(--text-secondary)', fontSize: '12px' }}>
                      {p.bat_pct}% · {p.age_s}s
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </div>

      {/* Right: MapLibre GL Topo Map */}
      <div class="panel" style={{ padding: '10px' }}>
        <div ref={containerRef} class="map-box" />
      </div>
    </div>
  );
}

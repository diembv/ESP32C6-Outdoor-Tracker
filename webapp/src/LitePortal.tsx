import { useEffect, useMemo, useState } from 'preact/hooks';
import { DeviceClient } from './core/device_client';
import { compileRouteBin, parseGpx } from './core/rdp_compiler';
import { convertCsvToGpx } from './core/csv_to_gpx';
import type { DeviceStatus, SdFileInfo, TeamConfig } from './core/types';

/**
 * Embedded Lite Portal (< 20 KB gzipped) served directly from ESP32 Flash at http://192.168.4.1
 */
export function LitePortal() {
  const client = useMemo(() => new DeviceClient('wifi', ''), []);
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [routes, setRoutes] = useState<SdFileInfo[]>([]);
  const [logs, setLogs] = useState<SdFileInfo[]>([]);
  const [team, setTeam] = useState<TeamConfig>({
    enabled: true,
    team_id: 2048,
    node_id: 1,
    callsign: 'USER',
    alert_dist_m: 250,
  });
  const [msg, setMsg] = useState('');

  async function refreshAll() {
    try {
      const [s, r, l, t] = await Promise.all([
        client.getStatus(),
        client.listRoutes(),
        client.listLogs(),
        client.getTeamConfig(),
      ]);
      setStatus(s);
      setRoutes(r);
      setLogs(l);
      setTeam(t);
    } catch (e) {
      setMsg(`Lỗi kết nối ESP32: ${(e as Error).message}`);
    }
  }

  useEffect(() => {
    refreshAll();
  }, []);

  async function handleGpxUpload(e: Event) {
    const file = (e.target as HTMLInputElement).files?.[0];
    if (!file) return;
    try {
      const text = await file.text();
      const parsed = parseGpx(text);
      const compiled = compileRouteBin(parsed.name, parsed.points, parsed.waypoints, 5.0);
      const binName = file.name.replace(/\.gpx$/i, '.bin');
      await client.uploadRoute(binName, compiled.buffer);
      setMsg(`Đã nén RDP (${compiled.header.point_count} điểm) & nạp ${binName}`);
      refreshAll();
    } catch (err) {
      setMsg(`Lỗi nạp GPX: ${(err as Error).message}`);
    }
  }

  async function handleExportGpx(name: string) {
    try {
      const csv = await client.downloadLog(name);
      const { gpxXml } = convertCsvToGpx(csv, name.replace(/\.csv$/i, ''));
      const blob = new Blob([gpxXml], { type: 'application/gpx+xml' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = name.replace(/\.csv$/i, '.gpx');
      a.click();
      URL.revokeObjectURL(url);
    } catch (err) {
      setMsg(`Lỗi xuất GPX: ${(err as Error).message}`);
    }
  }

  async function handleSaveTeam(e: Event) {
    e.preventDefault();
    try {
      await client.saveTeamConfig(team);
      setMsg('Đã lưu cấu hình nhóm!');
    } catch (err) {
      setMsg(`Lỗi lưu cấu hình: ${(err as Error).message}`);
    }
  }

  return (
    <div style="font-family:system-ui,sans-serif;background:#0b1015;color:#f1f5f9;min-height:100vh;padding:14px;max-width:680px;margin:0 auto">
      <header style="display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #263240;padding-bottom:10px;margin-bottom:14px">
        <div>
          <strong style="font-size:16px;color:#f59e0b">ESP32 Tracker Lite Portal</strong>
          <div style="font-size:12px;color:#94a3b8">
            {status ? `${status.board} · Pin ${status.battery_pct}% · SD ${status.sd_free_mb}MB` : 'Đang kết nối...'}
          </div>
        </div>
        <button onClick={refreshAll} style="padding:6px 10px;background:#1a242f;color:#fff;border:1px solid #334155;border-radius:6px">
          Làm mới
        </button>
      </header>

      {msg && (
        <div style="background:#1a242f;border-left:3px solid #f59e0b;padding:8px 12px;margin-bottom:12px;font-size:13px">
          {msg}
        </div>
      )}

      <section style="background:#121921;border:1px solid #263240;border-radius:10px;padding:12px;margin-bottom:12px">
        <h3 style="margin:0 0 8px;font-size:14px">Nạp Lộ Trình (.GPX → .BIN RDP)</h3>
        <input type="file" accept=".gpx" onChange={handleGpxUpload} />
        <ul style="list-style:none;padding:0;margin:10px 0 0;font-size:13px">
          {routes.map((r) => (
            <li key={r.name} style="display:flex;justify-content:space-between;padding:6px 0;border-top:1px solid #1e293b">
              <span>{r.name} {r.active ? '★' : ''}</span>
              {!r.active && (
                <button
                  onClick={() => client.activateRoute(r.name).then(refreshAll)}
                  style="padding:3px 8px;background:#f59e0b;color:#000;border:none;border-radius:4px;font-weight:600"
                >
                  Kích hoạt
                </button>
              )}
            </li>
          ))}
        </ul>
      </section>

      <section style="background:#121921;border:1px solid #263240;border-radius:10px;padding:12px;margin-bottom:12px">
        <h3 style="margin:0 0 8px;font-size:14px">Log Hành Trình (.CSV → GPX 1.1)</h3>
        <ul style="list-style:none;padding:0;margin:0;font-size:13px">
          {logs.map((l) => (
            <li key={l.name} style="display:flex;justify-content:space-between;padding:6px 0;border-top:1px solid #1e293b">
              <span>{l.name} ({(l.size_bytes / 1024).toFixed(1)} KB)</span>
              <button
                onClick={() => handleExportGpx(l.name)}
                style="padding:3px 8px;background:#10b981;color:#000;border:none;border-radius:4px;font-weight:600"
              >
                Tải GPX
              </button>
            </li>
          ))}
        </ul>
      </section>

      <section style="background:#121921;border:1px solid #263240;border-radius:10px;padding:12px">
        <h3 style="margin:0 0 8px;font-size:14px">Cấu Hình Nhóm (Team Beacon)</h3>
        <form onSubmit={handleSaveTeam} style="display:grid;grid-template-columns:1fr 1fr;gap:8px;font-size:13px">
          <label>
            Team ID
            <input
              type="number"
              value={team.team_id}
              onInput={(e) => setTeam({ ...team, team_id: Number((e.target as HTMLInputElement).value) })}
              style="width:100%;padding:6px;background:#0b1015;color:#fff;border:1px solid #334155;border-radius:4px"
            />
          </label>
          <label>
            Callsign
            <input
              maxLength={6}
              value={team.callsign}
              onInput={(e) => setTeam({ ...team, callsign: (e.target as HTMLInputElement).value.toUpperCase() })}
              style="width:100%;padding:6px;background:#0b1015;color:#fff;border:1px solid #334155;border-radius:4px"
            />
          </label>
          <button
            type="submit"
            style="grid-column:span 2;padding:8px;background:#f59e0b;color:#000;border:none;border-radius:6px;font-weight:700"
          >
            Lưu Cấu Hình Đội
          </button>
        </form>
      </section>
    </div>
  );
}

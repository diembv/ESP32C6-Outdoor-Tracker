import { useEffect, useState } from 'preact/hooks';
import { convertCsvToGpx } from '../core/csv_to_gpx';
import type { DeviceClient } from '../core/device_client';
import type { SdFileInfo } from '../core/types';

interface Props {
  client: DeviceClient;
  sdFreeMb?: number;
  onNotify: (msg: string) => void;
}

export function SdLogManager({ client, sdFreeMb, onNotify }: Props) {
  const [routes, setRoutes] = useState<SdFileInfo[]>([]);
  const [logs, setLogs] = useState<SdFileInfo[]>([]);
  const [loading, setLoading] = useState(false);

  async function refresh() {
    setLoading(true);
    try {
      const [r, l] = await Promise.all([client.listRoutes(), client.listLogs()]);
      setRoutes(r);
      setLogs(l);
    } catch (e) {
      onNotify(`Lỗi đọc thẻ nhớ: ${(e as Error).message}`);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    refresh();
  }, [client.mode]);

  async function handleActivateRoute(name: string) {
    try {
      await client.activateRoute(name);
      onNotify(`Đã kích hoạt lộ trình "${name}"`);
      await refresh();
    } catch (e) {
      onNotify(`Lỗi: ${(e as Error).message}`);
    }
  }

  async function handleExportGpx(name: string, shareMode = false) {
    try {
      const csvText = await client.downloadLog(name);
      const { gpxXml, stats } = convertCsvToGpx(csvText, name.replace(/\.csv$/i, ''));
      const gpxName = name.replace(/\.csv$/i, '.gpx');
      const file = new File([gpxXml], gpxName, { type: 'application/gpx+xml' });

      if (shareMode && navigator.share && navigator.canShare?.({ files: [file] })) {
        await navigator.share({
          title: gpxName,
          text: `Track ${stats.totalDistKm.toFixed(2)} km, +${stats.elevationGainM}m`,
          files: [file],
        });
        onNotify(`Đã chia sẻ ${gpxName}`);
        return;
      }

      const url = URL.createObjectURL(file);
      const a = document.createElement('a');
      a.href = url;
      a.download = gpxName;
      a.click();
      URL.revokeObjectURL(url);
      onNotify(
        `Đã xuất ${gpxName} (${stats.pointCount} điểm · ${stats.totalDistKm.toFixed(2)} km · +${stats.elevationGainM}m)`
      );
    } catch (e) {
      onNotify(`Lỗi xuất GPX: ${(e as Error).message}`);
    }
  }

  async function handleDeleteLog(name: string) {
    try {
      await client.deleteLog(name);
      onNotify(`Đã xóa ${name}`);
      await refresh();
    } catch (e) {
      onNotify(`Lỗi xóa file: ${(e as Error).message}`);
    }
  }

  return (
    <div class="grid-2">
      {/* Routes on MicroSD */}
      <div class="panel">
        <div class="panel-header">
          <div>
            <h2 class="panel-title">Lộ Trình Trên Thẻ Nhớ (/sdcard/routes/)</h2>
            <p class="panel-subtitle">Chuyển đổi cung đường dẫn đường mà không cần tháo thẻ</p>
          </div>
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            {sdFreeMb !== undefined && <span class="pill mono">Trống {sdFreeMb} MB</span>}
            <button class="btn btn-sm" onClick={refresh} disabled={loading}>
              Làm mới
            </button>
          </div>
        </div>

        {routes.length === 0 ? (
          <div style={{ color: 'var(--text-muted)' }}>Chưa có file .bin nào trên thẻ nhớ.</div>
        ) : (
          <table class="list-table">
            <thead>
              <tr>
                <th>Tên file</th>
                <th>Kích thước</th>
                <th style={{ textAlign: 'right' }}>Thao tác</th>
              </tr>
            </thead>
            <tbody>
              {routes.map((r) => (
                <tr key={r.name}>
                  <td class="mono" style={{ fontWeight: r.active ? 700 : 400 }}>
                    {r.name}
                    {r.active && (
                      <span
                        class="pill ok"
                        style={{ marginLeft: '8px', padding: '1px 7px', fontSize: '11px' }}
                      >
                        ACTIVE
                      </span>
                    )}
                  </td>
                  <td class="mono" style={{ color: 'var(--text-secondary)' }}>
                    {(r.size_bytes / 1024).toFixed(1)} KB
                  </td>
                  <td style={{ textAlign: 'right' }}>
                    {!r.active && (
                      <button
                        class="btn btn-sm btn-primary"
                        onClick={() => handleActivateRoute(r.name)}
                      >
                        Kích hoạt
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      {/* CSV Logs on MicroSD */}
      <div class="panel">
        <div class="panel-header">
          <div>
            <h2 class="panel-title">Nhật Ký Hành Trình (/sdcard/logs/)</h2>
            <p class="panel-subtitle">Biên dịch CSV → GPX 1.1 chuẩn Strava / Garmin Connect</p>
          </div>
        </div>

        {logs.length === 0 ? (
          <div style={{ color: 'var(--text-muted)' }}>Chưa có file log .csv nào trên thẻ nhớ.</div>
        ) : (
          <table class="list-table">
            <thead>
              <tr>
                <th>File Log</th>
                <th>Dung lượng</th>
                <th style={{ textAlign: 'right' }}>Xuất GPX 1.1</th>
              </tr>
            </thead>
            <tbody>
              {logs.map((l) => (
                <tr key={l.name}>
                  <td class="mono">{l.name}</td>
                  <td class="mono" style={{ color: 'var(--text-secondary)' }}>
                    {(l.size_bytes / 1024).toFixed(1)} KB
                  </td>
                  <td style={{ textAlign: 'right', display: 'flex', gap: '6px', justifyContent: 'flex-end' }}>
                    <button
                      class="btn btn-sm btn-emerald"
                      onClick={() => handleExportGpx(l.name, false)}
                    >
                      Tải GPX
                    </button>
                    {'share' in navigator && (
                      <button class="btn btn-sm" onClick={() => handleExportGpx(l.name, true)}>
                        Share
                      </button>
                    )}
                    <button class="btn btn-sm btn-danger" onClick={() => handleDeleteLog(l.name)}>
                      Xóa
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

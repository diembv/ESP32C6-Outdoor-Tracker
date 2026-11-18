import { useEffect, useState } from 'preact/hooks';
import type { DeviceClient } from '../core/device_client';
import type { TeamConfig } from '../core/types';

interface Props {
  client: DeviceClient;
  onNotify: (msg: string) => void;
}

export function TeamConfigPanel({ client, onNotify }: Props) {
  const [cfg, setCfg] = useState<TeamConfig>({
    enabled: true,
    team_id: 2048,
    node_id: 1,
    callsign: 'LEADER',
    alert_dist_m: 250,
  });
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    client
      .getTeamConfig()
      .then(setCfg)
      .catch(() => {});
  }, [client.mode]);

  async function handleSave(e: Event) {
    e.preventDefault();
    setSaving(true);
    try {
      await client.saveTeamConfig({
        ...cfg,
        callsign: cfg.callsign.toUpperCase().replace(/[^A-Z0-9_-]/g, '').slice(0, 6) || 'USER',
      });
      onNotify(`Đã lưu cấu hình Đội #${cfg.team_id} (${cfg.callsign}) vào NVS Flash!`);
    } catch (err) {
      onNotify(`Lỗi lưu cấu hình: ${(err as Error).message}`);
    } finally {
      setSaving(false);
    }
  }

  return (
    <div class="panel" style={{ maxWidth: '560px' }}>
      <div class="panel-header">
        <div>
          <h2 class="panel-title">Cấu Hình Ghép Đội (Team Beacon Provisioning)</h2>
          <p class="panel-subtitle">Đồng bộ Team ID, Callsign & Ngưỡng rớt đoàn xuống NVS Flash</p>
        </div>
      </div>

      <form onSubmit={handleSave}>
        <div class="field" style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <input
            id="team-enabled"
            type="checkbox"
            checked={cfg.enabled}
            onChange={(e) => setCfg({ ...cfg, enabled: (e.target as HTMLInputElement).checked })}
          />
          <label for="team-enabled" style={{ fontWeight: 600, cursor: 'pointer' }}>
            Bật phát sóng định vị đội (ESP-NOW Long-Range / LoRa)
          </label>
        </div>

        <div class="grid-2">
          <div class="field">
            <label class="field-label">Mã Phòng Nhóm (Team ID - 4 số)</label>
            <div style={{ display: 'flex', gap: '6px' }}>
              <input
                class="input mono"
                type="number"
                min={1000}
                max={9999}
                value={cfg.team_id}
                onInput={(e) => setCfg({ ...cfg, team_id: Number((e.target as HTMLInputElement).value) })}
              />
              <button
                type="button"
                class="btn btn-sm"
                onClick={() =>
                  setCfg({ ...cfg, team_id: Math.floor(1000 + Math.random() * 9000) })
                }
              >
                Ngẫu nhiên
              </button>
            </div>
          </div>

          <div class="field">
            <label class="field-label">Số Thứ Tự Máy (Node ID: 1–8)</label>
            <select
              class="select mono"
              value={cfg.node_id}
              onChange={(e) => setCfg({ ...cfg, node_id: Number((e.target as HTMLSelectElement).value) })}
            >
              {[1, 2, 3, 4, 5, 6, 7, 8].map((id) => (
                <option key={id} value={id}>
                  Node #{id} {id === 1 ? '(Trưởng đoàn)' : id === 8 ? '(Chốt đoàn)' : ''}
                </option>
              ))}
            </select>
          </div>
        </div>

        <div class="grid-2">
          <div class="field">
            <label class="field-label">Biệt Danh (Callsign, tối đa 6 ký tự)</label>
            <input
              class="input mono"
              maxLength={6}
              value={cfg.callsign}
              onInput={(e) =>
                setCfg({
                  ...cfg,
                  callsign: (e.target as HTMLInputElement).value.toUpperCase().slice(0, 6),
                })
              }
            />
          </div>

          <div class="field">
            <label class="field-label">Cảnh báo rớt đoàn ({cfg.alert_dist_m} m)</label>
            <input
              type="range"
              min={50}
              max={1000}
              step={25}
              value={cfg.alert_dist_m}
              style={{ width: '100%', marginTop: '8px' }}
              onInput={(e) =>
                setCfg({ ...cfg, alert_dist_m: Number((e.target as HTMLInputElement).value) })
              }
            />
          </div>
        </div>

        <button class="btn btn-primary" type="submit" disabled={saving} style={{ marginTop: '6px' }}>
          {saving ? 'Đang ghi NVS...' : 'Lưu & Đồng Bộ Lên ESP32'}
        </button>
      </form>
    </div>
  );
}

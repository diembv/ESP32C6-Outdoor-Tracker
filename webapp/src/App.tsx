import { useEffect, useMemo, useState } from 'preact/hooks';
import { DeviceClient } from './core/device_client';
import type { CompiledRoute, ConnectionMode, DeviceStatus, PeerInfo } from './core/types';
import { RouteStudio } from './components/RouteStudio';
import { TopoMapView } from './components/TopoMapView';
import { SdLogManager } from './components/SdLogManager';
import { TeamConfigPanel } from './components/TeamConfigPanel';
import { TeamReplayView } from './components/TeamReplayView';

type TabId = 'studio' | 'map' | 'sd' | 'replay';

export function App() {
  const client = useMemo(() => new DeviceClient('demo', ''), []);
  const [mode, setMode] = useState<ConnectionMode>('demo');
  const [wifiUrl, setWifiUrl] = useState('http://192.168.4.1');
  const [tab, setTab] = useState<TabId>('studio');

  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [peers, setPeers] = useState<PeerInfo[]>([]);
  const [activeRoute, setActiveRoute] = useState<CompiledRoute | null>(null);
  const [toast, setToast] = useState<string | null>(null);

  function notify(msg: string) {
    setToast(msg);
  }

  useEffect(() => {
    if (!toast) return;
    const t = setTimeout(() => setToast(null), 3500);
    return () => clearTimeout(t);
  }, [toast]);

  async function pollDevice() {
    try {
      const st = await client.getStatus();
      setStatus(st);
      const pr = await client.getPeers(st.lat, st.lon, st.baro_alt_m);
      setPeers(pr);
    } catch {
      // Keep last status on transient error
    }
  }

  useEffect(() => {
    pollDevice();
    const id = setInterval(pollDevice, 2000);
    return () => clearInterval(id);
  }, [mode]);

  async function handleModeChange(nextMode: ConnectionMode) {
    if (nextMode === 'ble') {
      try {
        const name = await client.connectBle();
        setMode('ble');
        notify(`Đã kết nối BLE: ${name}`);
      } catch (e) {
        notify((e as Error).message);
      }
      return;
    }
    client.disconnectBle();
    client.setMode(nextMode, nextMode === 'wifi' ? wifiUrl : '');
    setMode(nextMode);
    notify(
      nextMode === 'wifi'
        ? `Chuyển sang Wi-Fi AP (${wifiUrl})`
        : 'Đang chạy chế độ Demo Simulator'
    );
  }

  return (
    <div class="app-shell">
      {/* Top Bar */}
      <header class="topbar">
        <div class="topbar-inner">
          <div class="brand">
            <div class="brand-mark">C6</div>
            <div>
              <h1 class="brand-title">ESP32 Outdoor Tracker</h1>
              <div class="brand-sub">Companion Topo Studio & Team Bridge</div>
            </div>
          </div>

          <div class="telemetry-strip">
            {status && (
              <>
                <span class={`pill mono ${status.gps_fix ? 'ok' : 'warn'}`}>
                  GPS {status.gps_fix ? `${status.satellites} SAT` : 'NO FIX'}
                </span>
                <span class="pill mono">
                  Baro <strong>{status.baro_alt_m.toFixed(0)}m</strong>
                </span>
                <span class="pill mono">
                  Pin <strong>{status.battery_pct}%</strong>
                </span>
                {status.sos_active && <span class="pill danger mono">SOS ACTIVE</span>}
              </>
            )}

            <select
              class="select"
              style={{ width: '165px', padding: '5px 8px', fontSize: '12px' }}
              value={mode}
              onChange={(e) => handleModeChange((e.target as HTMLSelectElement).value as ConnectionMode)}
            >
              <option value="demo">Demo Simulator</option>
              <option value="wifi">Wi-Fi AP (192.168.4.1)</option>
              <option value="ble">WebBLE (NimBLE)</option>
            </select>

            {mode === 'wifi' && (
              <input
                class="input mono"
                style={{ width: '155px', padding: '5px 8px', fontSize: '12px' }}
                value={wifiUrl}
                onChange={(e) => {
                  const val = (e.target as HTMLInputElement).value;
                  setWifiUrl(val);
                  client.setMode('wifi', val);
                }}
              />
            )}
          </div>
        </div>
      </header>

      {/* Navigation Tabs */}
      <nav class="nav-bar">
        <div class="nav-tabs">
          <button
            class={`nav-tab ${tab === 'studio' ? 'active' : ''}`}
            onClick={() => setTab('studio')}
          >
            Route Studio & Offline Topo
          </button>
          <button
            class={`nav-tab ${tab === 'map' ? 'active' : ''}`}
            onClick={() => setTab('map')}
          >
            Bản Đồ & Đồng Đội ({peers.length})
          </button>
          <button
            class={`nav-tab ${tab === 'sd' ? 'active' : ''}`}
            onClick={() => setTab('sd')}
          >
            Thẻ Nhớ SD & Cấu Hình Đội
          </button>
          <button
            class={`nav-tab ${tab === 'replay' ? 'active' : ''}`}
            onClick={() => setTab('replay')}
          >
            Team Replay & Thống Kê
          </button>
        </div>
      </nav>

      {/* Active Page */}
      <main class="main-content">
        {tab === 'studio' && (
          <RouteStudio client={client} onCompiledRoute={setActiveRoute} onNotify={notify} />
        )}
        {tab === 'map' && (
          <TopoMapView
            client={client}
            status={status}
            peers={peers}
            activeRoute={activeRoute}
            onRefresh={pollDevice}
            onNotify={notify}
          />
        )}
        {tab === 'sd' && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            <SdLogManager client={client} sdFreeMb={status?.sd_free_mb} onNotify={notify} />
            <TeamConfigPanel client={client} onNotify={notify} />
          </div>
        )}
        {tab === 'replay' && <TeamReplayView onNotify={notify} />}
      </main>

      {toast && <div class="toast">{toast}</div>}
    </div>
  );
}

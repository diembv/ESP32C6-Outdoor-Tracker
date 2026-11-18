import { bearingDeg, haversineDistanceM } from './rdp_compiler';
import type {
  ConnectionMode,
  DeviceStatus,
  PeerInfo,
  SdFileInfo,
  TeamConfig,
} from './types';

export const BLE_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
export const BLE_CHAR_TELEMETRY = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
export const BLE_CHAR_PEERS = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
export const BLE_CHAR_CONTROL = '6e400004-b5a3-f393-e0a9-e50e24dcca9e';

export type RemoteCommand =
  | { type: 'toggle_log' }
  | { type: 'calib_baro' }
  | { type: 'set_mode'; mode: 'HIKE' | 'BIKE' }
  | { type: 'toggle_sos'; active: boolean };

/**
 * Unified Device Client supporting:
 *  - Wi-Fi AP HTTP REST API (http://192.168.4.1 or relative /)
 *  - WebBLE (NimBLE GATT Profile)
 *  - Demo Simulator (for offline testing without physical hardware)
 */
export class DeviceClient {
  public mode: ConnectionMode = 'demo';
  public baseUrl = '';
  private bleDevice: BluetoothDevice | null = null;
  private bleControlChar: BluetoothRemoteGATTCharacteristic | null = null;

  // Demo simulation state
  private demoTick = 0;
  private demoStatus: DeviceStatus = {
    board: 'ESP32-C6 AMOLED 1.64" (SIM)',
    battery_v: 4.02,
    battery_pct: 84,
    gps_fix: true,
    satellites: 11,
    hdop: 0.9,
    lat: 21.0748,
    lon: 105.3652,
    gps_alt_m: 1124,
    baro_alt_m: 1126.4,
    baro_calibrated: true,
    speed_kmh: 3.4,
    course_deg: 42,
    sd_ok: true,
    sd_free_mb: 14820,
    active_route: 'bavi_summit.bin',
    is_logging: true,
    activity_mode: 'HIKE',
    sos_active: false,
    uptime_s: 4380,
  };

  private demoTeam: TeamConfig = {
    enabled: true,
    team_id: 2048,
    node_id: 1,
    callsign: 'LEADER',
    alert_dist_m: 250,
  };

  private demoRoutes: SdFileInfo[] = [
    { name: 'bavi_summit.bin', size_bytes: 1840, active: true },
    { name: 'nhiu_co_san_up.bin', size_bytes: 4920, active: false },
    { name: 'ky_quan_san_ridge.bin', size_bytes: 6310, active: false },
  ];

  private demoLogs: Map<string, string> = new Map([
    [
      '20261117_073012.csv',
      [
        'ts,lat,lon,alt',
        '1763364612,21.070000,105.362000,420.5',
        '1763364672,21.070850,105.362600,445.2',
        '1763364732,21.071600,105.363100,478.8',
        '1763364792,21.072400,105.363700,515.4',
        '1763364852,21.073100,105.364200,562.0',
        '1763364912,21.073900,105.364700,618.3',
        '1763364972,21.074800,105.365200,680.1',
      ].join('\n'),
    ],
    [
      '20261116_141500.csv',
      [
        'ts,lat,lon,alt',
        '1763302500,21.068000,105.360000,380.0',
        '1763302560,21.068600,105.360500,398.4',
        '1763302620,21.069200,105.361100,415.9',
      ].join('\n'),
    ],
  ]);

  constructor(mode: ConnectionMode = 'demo', baseUrl = '') {
    this.mode = mode;
    this.baseUrl = baseUrl;
  }

  public setMode(mode: ConnectionMode, baseUrl = '') {
    this.mode = mode;
    if (baseUrl !== undefined) {
      this.baseUrl = baseUrl.replace(/\/$/, '');
    }
  }

  public async connectBle(): Promise<string> {
    if (!navigator.bluetooth) {
      throw new Error('Trình duyệt không hỗ trợ Web Bluetooth (hãy dùng Chrome/Edge trên Android hoặc Desktop)');
    }
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [BLE_SERVICE_UUID] }],
    });
    const server = await device.gatt?.connect();
    if (!server) throw new Error('Không thể kết nối GATT server');
    const service = await server.getPrimaryService(BLE_SERVICE_UUID);
    this.bleControlChar = await service.getCharacteristic(BLE_CHAR_CONTROL);
    this.bleDevice = device;
    this.mode = 'ble';
    return device.name || 'ESP32-Tracker';
  }

  public disconnectBle() {
    if (this.bleDevice?.gatt?.connected) {
      this.bleDevice.gatt.disconnect();
    }
    this.bleDevice = null;
    this.bleControlChar = null;
  }

  public async getStatus(): Promise<DeviceStatus> {
    if (this.mode === 'demo') {
      this.demoTick++;
      const driftLat = Math.sin(this.demoTick * 0.15) * 0.00018;
      const driftLon = Math.cos(this.demoTick * 0.12) * 0.00018;
      this.demoStatus = {
        ...this.demoStatus,
        lat: 21.0748 + driftLat,
        lon: 105.3652 + driftLon,
        baro_alt_m: 1126.4 + Math.sin(this.demoTick * 0.1) * 2.5,
        uptime_s: this.demoStatus.uptime_s + 2,
      };
      return { ...this.demoStatus };
    }

    const res = await fetch(`${this.baseUrl}/api/status`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  public async getPeers(selfLat?: number, selfLon?: number, selfAlt?: number): Promise<PeerInfo[]> {
    let peers: PeerInfo[];
    if (this.mode === 'demo') {
      const t = this.demoTick * 0.12;
      peers = [
        {
          node_id: 2,
          callsign: 'DIEM',
          lat: 21.0742 + Math.sin(t) * 0.00025,
          lon: 105.3646 + Math.cos(t) * 0.0002,
          alt_m: 1108,
          bat_pct: 79,
          sos: false,
          age_s: 2,
          rssi: -68,
        },
        {
          node_id: 3,
          callsign: 'PORTER',
          lat: 21.0731 + Math.cos(t * 0.8) * 0.0002,
          lon: 105.3638 + Math.sin(t * 0.8) * 0.0002,
          alt_m: 1064,
          bat_pct: 62,
          sos: false,
          age_s: 5,
          rssi: -79,
        },
        {
          node_id: 4,
          callsign: 'SWEEP',
          lat: 21.0718,
          lon: 105.3629,
          alt_m: 992,
          bat_pct: 41,
          sos: false,
          age_s: 14,
          rssi: -88,
        },
      ];
    } else {
      const res = await fetch(`${this.baseUrl}/api/peers`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      peers = await res.json();
    }

    if (selfLat !== undefined && selfLon !== undefined) {
      peers = peers.map((p) => ({
        ...p,
        dist_m: Math.round(haversineDistanceM(selfLat, selfLon, p.lat, p.lon)),
        bearing_deg: Math.round(bearingDeg(selfLat, selfLon, p.lat, p.lon)),
        delta_alt_m: selfAlt !== undefined ? Math.round(p.alt_m - selfAlt) : undefined,
      }));
    }
    return peers;
  }

  public async listLogs(): Promise<SdFileInfo[]> {
    if (this.mode === 'demo') {
      return Array.from(this.demoLogs.entries()).map(([name, content]) => ({
        name,
        size_bytes: content.length,
      }));
    }
    const res = await fetch(`${this.baseUrl}/api/logs`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  public async downloadLog(name: string): Promise<string> {
    if (this.mode === 'demo') {
      const data = this.demoLogs.get(name);
      if (!data) throw new Error(`Không tìm thấy ${name}`);
      return data;
    }
    const res = await fetch(`${this.baseUrl}/api/logs/download?name=${encodeURIComponent(name)}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.text();
  }

  public async deleteLog(name: string): Promise<void> {
    if (this.mode === 'demo') {
      this.demoLogs.delete(name);
      return;
    }
    const res = await fetch(`${this.baseUrl}/api/logs?name=${encodeURIComponent(name)}`, {
      method: 'DELETE',
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
  }

  public async listRoutes(): Promise<SdFileInfo[]> {
    if (this.mode === 'demo') {
      return this.demoRoutes.map((r) => ({ ...r }));
    }
    const res = await fetch(`${this.baseUrl}/api/routes`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  public async uploadRoute(name: string, binData: Uint8Array): Promise<void> {
    const cleanName = name.endsWith('.bin') ? name : `${name}.bin`;
    if (this.mode === 'demo') {
      this.demoRoutes.forEach((r) => (r.active = false));
      const existing = this.demoRoutes.find((r) => r.name === cleanName);
      if (existing) {
        existing.size_bytes = binData.byteLength;
        existing.active = true;
      } else {
        this.demoRoutes.unshift({ name: cleanName, size_bytes: binData.byteLength, active: true });
      }
      this.demoStatus.active_route = cleanName;
      return;
    }

    const res = await fetch(`${this.baseUrl}/api/routes/upload?name=${encodeURIComponent(cleanName)}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/octet-stream' },
      body: new Blob([binData.buffer as ArrayBuffer]),
    });
    if (!res.ok) throw new Error(`Upload failed: HTTP ${res.status}`);
  }

  public async activateRoute(name: string): Promise<void> {
    if (this.mode === 'demo') {
      this.demoRoutes.forEach((r) => (r.active = r.name === name));
      this.demoStatus.active_route = name;
      return;
    }
    const res = await fetch(`${this.baseUrl}/api/routes/activate?name=${encodeURIComponent(name)}`, {
      method: 'POST',
    });
    if (!res.ok) throw new Error(`Activate failed: HTTP ${res.status}`);
  }

  public async getTeamConfig(): Promise<TeamConfig> {
    if (this.mode === 'demo') {
      return { ...this.demoTeam };
    }
    const res = await fetch(`${this.baseUrl}/api/team`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.json();
  }

  public async saveTeamConfig(cfg: TeamConfig): Promise<void> {
    if (this.mode === 'demo') {
      this.demoTeam = { ...cfg };
      return;
    }
    const res = await fetch(`${this.baseUrl}/api/team`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(cfg),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
  }

  public async sendCommand(cmd: RemoteCommand): Promise<void> {
    if (this.mode === 'demo') {
      if (cmd.type === 'toggle_log') this.demoStatus.is_logging = !this.demoStatus.is_logging;
      if (cmd.type === 'calib_baro') this.demoStatus.baro_calibrated = true;
      if (cmd.type === 'set_mode') this.demoStatus.activity_mode = cmd.mode;
      if (cmd.type === 'toggle_sos') this.demoStatus.sos_active = cmd.active;
      return;
    }

    if (this.mode === 'ble' && this.bleControlChar) {
      let payload: Uint8Array;
      switch (cmd.type) {
        case 'toggle_log':
          payload = new Uint8Array([0x01]);
          break;
        case 'calib_baro':
          payload = new Uint8Array([0x02]);
          break;
        case 'set_mode':
          payload = new Uint8Array([0x03, cmd.mode === 'BIKE' ? 1 : 0]);
          break;
        case 'toggle_sos':
          payload = new Uint8Array([0x05, cmd.active ? 1 : 0]);
          break;
      }
      await this.bleControlChar.writeValue(payload as unknown as BufferSource);
      return;
    }

    await fetch(`${this.baseUrl}/api/cmd`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(cmd),
    });
  }
}

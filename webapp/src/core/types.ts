export type WaypointIcon = 0 | 1 | 2 | 3 | 4;

export const WAYPOINT_ICONS: Record<WaypointIcon, { label: string; badge: string; color: string }> = {
  0: { label: 'Mốc chung', badge: 'WPT', color: '#94a3b8' },
  1: { label: 'Đỉnh núi', badge: 'SUMMIT', color: '#f59e0b' },
  2: { label: 'Lán nghỉ / Camp', badge: 'CAMP', color: '#10b981' },
  3: { label: 'Suối / Nước', badge: 'WATER', color: '#38bdf8' },
  4: { label: 'Nguy hiểm', badge: 'DANGER', color: '#ef4444' },
};

export interface GeoPoint {
  lat: number;
  lon: number;
  ele?: number;
  ts?: number;
}

export interface RouteWaypoint {
  lat: number;
  lon: number;
  alt_m: number;
  icon_type: WaypointIcon;
  label: string; // max 20 ASCII chars + null terminator (21 bytes)
}

export interface RouteHeader {
  magic: string;        // "ROUT"
  version: number;      // 1 or 2
  waypoint_count: number;
  point_count: number;
  total_dist_km: number;
  min_lat: number;
  max_lat: number;
  min_lon: number;
  max_lon: number;
  name: string;         // max 31 bytes + null
}

export interface CompiledRoute {
  header: RouteHeader;
  points: GeoPoint[];
  waypoints: RouteWaypoint[];
  buffer: Uint8Array;
}

export interface DeviceStatus {
  board: string;
  battery_v: number;
  battery_pct: number;
  gps_fix: boolean;
  satellites: number;
  hdop: number;
  lat: number;
  lon: number;
  gps_alt_m: number;
  baro_alt_m: number;
  baro_calibrated: boolean;
  speed_kmh: number;
  course_deg: number;
  sd_ok: boolean;
  sd_free_mb: number;
  active_route: string;
  is_logging: boolean;
  activity_mode: 'HIKE' | 'BIKE';
  sos_active: boolean;
  uptime_s: number;
}

export interface PeerInfo {
  node_id: number;
  callsign: string;
  lat: number;
  lon: number;
  alt_m: number;
  bat_pct: number;
  sos: boolean;
  age_s: number;
  rssi?: number;
  dist_m?: number;
  bearing_deg?: number;
  delta_alt_m?: number;
}

export interface SdFileInfo {
  name: string;
  size_bytes: number;
  active?: boolean;
  modified?: string;
}

export interface TeamConfig {
  enabled: boolean;
  team_id: number;     // 4-digit code (e.g. 1024 - 9999)
  node_id: number;     // 1..8
  callsign: string;    // max 6 chars
  alert_dist_m: number;
}

export type ConnectionMode = 'demo' | 'wifi' | 'ble';

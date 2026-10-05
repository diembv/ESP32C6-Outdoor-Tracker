#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
=============================================================================
ESP32-C6 Outdoor Tracker: Compact CSV to GPX 1.1 Converter
=============================================================================
Chuyển đổi file log Compact CSV siêu nhẹ (ts,lat,lon,alt) sang chuẩn GPX 1.1
để tải lên Strava, Garmin Connect, Google Earth, GPX Studio,...

Cách sử dụng:
  1. Kéo-thả file .csv vào script này
  2. Hoặc chạy qua dòng lệnh (CLI):
       python tools/csv_to_gpx.py <file1.csv> [file2.csv ...]
       python tools/csv_to_gpx.py path/to/20261005_145211.csv -o my_track.gpx
=============================================================================
"""

import sys
import os
import csv
import math
import argparse
from datetime import datetime, timezone

# Khắc phục lỗi font console Windows (cp1252 UnicodeEncodeError)
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

def haversine_distance(lat1, lon1, lat2, lon2):
    """Tính khoảng cách (mét) giữa 2 tọa độ GPS theo công thức Haversine."""
    R = 6371000.0  # Bán kính Trái Đất (mét)
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    delta_phi = math.radians(lat2 - lat1)
    delta_lambda = math.radians(lon2 - lon1)
    a = (math.sin(delta_phi / 2.0) ** 2 +
         math.cos(phi1) * math.cos(phi2) * (math.sin(delta_lambda / 2.0) ** 2))
    c = 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))
    return R * c

def convert_csv_to_gpx(csv_path, output_path=None):
    if not os.path.isfile(csv_path):
        print(f"[!] Lỗi: Không tìm thấy file: {csv_path}")
        return False

    if output_path is None:
        base, _ = os.path.splitext(csv_path)
        output_path = base + ".gpx"

    print(f"\n[*] Đang xử lý: {os.path.basename(csv_path)}")

    trackpoints = []
    skipped_rows = 0

    with open(csv_path, "r", encoding="utf-8", errors="ignore") as f:
        reader = csv.reader(f)
        header = None

        for row_idx, row in enumerate(reader):
            if not row or all(c.strip() == "" for c in row):
                continue

            # Xử lý dòng tiêu đề (Header)
            if header is None:
                cleaned = [col.strip().lower() for col in row]
                if "ts" in cleaned or "lat" in cleaned:
                    header = cleaned
                    continue
                else:
                    # File không có header -> giả định cột mặc định [ts, lat, lon, alt]
                    header = ["ts", "lat", "lon", "alt"]

            # Phân tích các cột
            try:
                # Nếu có header chuẩn, map theo tên cột
                if "ts" in header and "lat" in header and "lon" in header:
                    col_ts = header.index("ts")
                    col_lat = header.index("lat")
                    col_lon = header.index("lon")
                    col_alt = header.index("alt") if "alt" in header else -1

                    ts_val = float(row[col_ts].strip())
                    lat_val = float(row[col_lat].strip())
                    lon_val = float(row[col_lon].strip())
                    alt_val = float(row[col_alt].strip()) if (col_alt >= 0 and col_alt < len(row)) else None
                else:
                    # Mặc định theo thứ tự 0: ts, 1: lat, 2: lon, 3: alt
                    ts_val = float(row[0].strip())
                    lat_val = float(row[1].strip())
                    lon_val = float(row[2].strip())
                    alt_val = float(row[3].strip()) if len(row) > 3 else None

                # Kiểm tra tính hợp lệ của tọa độ
                if not (-90.0 <= lat_val <= 90.0 and -180.0 <= lon_val <= 180.0):
                    skipped_rows += 1
                    continue

                trackpoints.append({
                    "ts": ts_val,
                    "lat": lat_val,
                    "lon": lon_val,
                    "alt": alt_val
                })
            except (ValueError, IndexError):
                skipped_rows += 1
                continue

    if not trackpoints:
        print("[!] Cảnh báo: Không có điểm tọa độ hợp lệ nào được tìm thấy trong file.")
        return False

    # Thống kê lộ trình
    first_pt = trackpoints[0]
    last_pt = trackpoints[-1]

    dt_start = datetime.fromtimestamp(first_pt["ts"], tz=timezone.utc)
    dt_end = datetime.fromtimestamp(last_pt["ts"], tz=timezone.utc)
    duration_s = max(0, int(last_pt["ts"] - first_pt["ts"]))

    total_dist_m = 0.0
    for i in range(1, len(trackpoints)):
        p1 = trackpoints[i - 1]
        p2 = trackpoints[i]
        total_dist_m += haversine_distance(p1["lat"], p1["lon"], p2["lat"], p2["lon"])

    time_meta_str = dt_start.strftime("%Y-%m-%dT%H:%M:%SZ")
    track_name = f"Track {dt_start.strftime('%Y%m%d_%H%M%S')}"

    # Ghi file GPX 1.1
    with open(output_path, "w", encoding="utf-8") as out:
        out.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        out.write('<gpx version="1.1" creator="ESP32-C6 Outdoor Tracker (tools/csv_to_gpx.py)"\n')
        out.write('     xmlns="http://www.topografix.com/GPX/1/1"\n')
        out.write('     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"\n')
        out.write('     xsi:schemaLocation="http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd">\n')
        out.write('  <metadata>\n')
        out.write(f'    <name>{track_name}</name>\n')
        out.write(f'    <time>{time_meta_str}</time>\n')
        out.write('  </metadata>\n')
        out.write('  <trk>\n')
        out.write(f'    <name>{track_name}</name>\n')
        out.write('    <trkseg>\n')

        for pt in trackpoints:
            iso_time = datetime.fromtimestamp(pt["ts"], tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            out.write(f'      <trkpt lat="{pt["lat"]:.6f}" lon="{pt["lon"]:.6f}">\n')
            if pt["alt"] is not None:
                out.write(f'        <ele>{pt["alt"]:.1f}</ele>\n')
            out.write(f'        <time>{iso_time}</time>\n')
            out.write('      </trkpt>\n')

        out.write('    </trkseg>\n')
        out.write('  </trk>\n')
        out.write('</gpx>\n')

    csv_size = os.path.getsize(csv_path)
    gpx_size = os.path.getsize(output_path)

    print(f" [OK] Xuất thành công: {output_path}")
    print(f"      - Tổng số điểm ghi  : {len(trackpoints)} điểm (bỏ qua: {skipped_rows})")
    print(f"      - Bắt đầu           : {dt_start.strftime('%Y-%m-%d %H:%M:%S')} UTC")
    print(f"      - Kết thúc          : {dt_end.strftime('%Y-%m-%d %H:%M:%S')} UTC")
    print(f"      - Thời lượng        : {duration_s // 60}m {duration_s % 60}s")
    print(f"      - Quãng đường ước tính: {total_dist_m / 1000.0:.2f} km")
    print(f"      - Kích thước file CSV: {csv_size:,} bytes")
    print(f"      - Kích thước file GPX: {gpx_size:,} bytes (CSV tiết kiệm được {max(0, 100 - (csv_size*100//gpx_size))}%)")

    return True

def main():
    parser = argparse.ArgumentParser(
        description="Chuyển đổi file log Compact CSV từ ESP32-C6 Outdoor Tracker sang GPX 1.1 chuẩn quốc tế."
    )
    parser.add_argument("files", nargs="*", help="Đường dẫn file .csv cần chuyển đổi (hỗ trợ nhiều file).")
    parser.add_argument("-o", "--output", help="Đường dẫn file .gpx đầu ra (khi chỉ chuyển 1 file).")

    args = parser.parse_args()

    # Nếu không truyền file qua CLI (ví dụ kéo-thả hoặc nhấp đúp chạy trực tiếp)
    target_files = args.files
    if not target_files:
        print("=================================================================")
        print(" ESP32-C6 Tracker: Compact CSV -> GPX 1.1 Converter")
        print("=================================================================")
        inp = input("Kéo thả file CSV vào đây hoặc nhập đường dẫn (bấm Enter để thoát): ").strip(' "\'')
        if inp:
            target_files = [inp]
        else:
            print("[*] Kết thúc.")
            return

    for fpath in target_files:
        out_name = args.output if (len(target_files) == 1 and args.output) else None
        convert_csv_to_gpx(fpath, out_name)

if __name__ == "__main__":
    main()

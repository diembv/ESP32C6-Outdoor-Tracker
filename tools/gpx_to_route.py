#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
=============================================================================
ESP32-C6 Outdoor Tracker: GPX to Compact Route Binary Converter
=============================================================================
Chuyển đổi file lộ trình GPX (.gpx) sang định dạng Binary siêu nhẹ (.bin)
cho bộ định vị ESP32-C6 (Waveshare Touch AMOLED 1.64").

Tính năng nổi bật:
  - Nén vệt tọa độ bằng thuật toán Ramer-Douglas-Peucker (RDP)
  - Giảm 70-90% số điểm thừa mà vẫn giữ nguyên từng góc cua/khúc ngoặt
  - Định dạng Fixed-size Record 8 bytes/điểm (float lat, float lon)
  - Tích hợp Header 96 bytes chứa Bounds (min/max), tổng cự ly và tên route
  - Cho phép ESP32 fseek đọc cuộn (Sliding Window) trực tiếp từ thẻ MicroSD
  - Chế độ --demo để tạo file lộ trình mẫu test ngay lập tức

Cách sử dụng:
  1. Kéo-thả file .gpx vào script này
  2. Hoặc chạy qua dòng lệnh:
       python tools/gpx_to_route.py my_route.gpx
       python tools/gpx_to_route.py my_route.gpx -o /sdcard/routes/active.bin --epsilon 5.0
       python tools/gpx_to_route.py --demo
=============================================================================
"""

import sys
import os
import math
import struct
import argparse
import xml.etree.ElementTree as ET

# Khắc phục lỗi font console Windows (cp1252 UnicodeEncodeError)
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

HEADER_MAGIC = b"ROUT"
HEADER_VERSION = 1
HEADER_SIZE = 96
POINT_SIZE = 8

def haversine_distance(lat1, lon1, lat2, lon2):
    """Tính khoảng cách (mét) giữa 2 điểm GPS theo công thức Haversine."""
    R = 6371000.0  # Bán kính Trái Đất (mét)
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    delta_phi = math.radians(lat2 - lat1)
    delta_lambda = math.radians(lon2 - lon1)
    a = (math.sin(delta_phi / 2.0) ** 2 +
         math.cos(phi1) * math.cos(phi2) * (math.sin(delta_lambda / 2.0) ** 2))
    c = 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))
    return R * c

def perpendicular_distance(pt, line_start, line_end):
    """
    Tính khoảng cách vuông góc (mét) từ điểm pt tới đoạn thẳng (line_start, line_end).
    Dùng phép chiếu phẳng Equirectangular cục bộ.
    """
    lat, lon = pt
    lat1, lon1 = line_start
    lat2, lon2 = line_end

    # Chiếu sang tọa độ mét x, y so với line_start
    mid_lat = math.radians((lat1 + lat2 + lat) / 3.0)
    kx = 111320.0 * math.cos(mid_lat)
    ky = 111320.0

    x = (lon - lon1) * kx
    y = (lat - lat1) * ky

    x2 = (lon2 - lon1) * kx
    y2 = (lat2 - lat1) * ky

    line_len_sq = x2 * x2 + y2 * y2
    if line_len_sq == 0:
        return math.sqrt(x * x + y * y)

    # Chiếu pt lên vector (x2, y2)
    t = max(0.0, min(1.0, (x * x2 + y * y2) / line_len_sq))
    proj_x = t * x2
    proj_y = t * y2

    dx = x - proj_x
    dy = y - proj_y
    return math.sqrt(dx * dx + dy * dy)

def ramer_douglas_peucker(points, epsilon):
    """
    Thuật toán rút gọn đường gấp khúc Ramer-Douglas-Peucker (RDP).
    points: danh sách các tuple (lat, lon)
    epsilon: ngưỡng dung sai sai lệch tối đa cho phép (mét)
    """
    if len(points) < 3:
        return points

    dmax = 0.0
    index = 0
    end = len(points) - 1

    for i in range(1, end):
        d = perpendicular_distance(points[i], points[0], points[end])
        if d > dmax:
            index = i
            dmax = d

    if dmax > epsilon:
        # Đệ quy chia đôi
        rec1 = ramer_douglas_peucker(points[:index + 1], epsilon)
        rec2 = ramer_douglas_peucker(points[index:], epsilon)
        return rec1[:-1] + rec2
    else:
        return [points[0], points[end]]

def parse_gpx(gpx_path):
    """Phân tích cú pháp GPX, hỗ trợ trkpt và rtept không phụ thuộc namespace XML."""
    tree = ET.parse(gpx_path)
    root = tree.getroot()

    points = []
    route_name = "Route"

    # Tìm tên lộ trình nếu có
    for elem in root.iter():
        tag = elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag
        if tag == "name" and elem.text and not route_name.startswith("GPX"):
            route_name = elem.text.strip()
            break

    # Thu thập tất cả các trackpoint hoặc routepoint
    for elem in root.iter():
        tag = elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag
        if tag in ("trkpt", "rtept"):
            try:
                lat = float(elem.attrib["lat"])
                lon = float(elem.attrib["lon"])
                points.append((lat, lon))
            except (KeyError, ValueError):
                continue

    return route_name, points

def write_binary_route(output_path, route_name, points):
    """
    Ghi dữ liệu nhị phân với header 96 bytes và danh sách điểm 8 bytes.
    """
    if not points:
        return False

    # Tính bounds và tổng quãng đường
    min_lat = min(p[0] for p in points)
    max_lat = max(p[0] for p in points)
    min_lon = min(p[1] for p in points)
    max_lon = max(p[1] for p in points)

    total_dist_m = 0.0
    for i in range(len(points) - 1):
        total_dist_m += haversine_distance(points[i][0], points[i][1],
                                           points[i + 1][0], points[i + 1][1])
    total_dist_km = total_dist_m / 1000.0

    # Chuẩn bị name buffer (32 bytes)
    clean_name = os.path.basename(route_name)[:31].encode("utf-8", errors="ignore")
    name_buf = clean_name.ljust(32, b"\x00")

    # Header 96 bytes:
    # magic (4s), version (H), reserved1 (H), point_count (I), total_dist_km (f),
    # min_lat (f), max_lat (f), min_lon (f), max_lon (f), name (32s), reserved (32s)
    header = struct.pack(
        "<4sHHIf4f32s32s",
        HEADER_MAGIC,
        HEADER_VERSION,
        0,                      # reserved1
        len(points),            # point_count (uint32)
        total_dist_km,          # total_dist_km (float)
        min_lat,
        max_lat,
        min_lon,
        max_lon,
        name_buf,
        b"\x00" * 32            # reserved
    )

    with open(output_path, "wb") as f:
        f.write(header)
        for lat, lon in points:
            f.write(struct.pack("<ff", float(lat), float(lon)))

    return total_dist_km, min_lat, max_lat, min_lon, max_lon

def create_demo_gpx(demo_path):
    """Tạo một file GPX trekking mẫu (cung đường Đền Thượng - Ba Vì) để test."""
    # Điểm xuất phát quanh chân núi Ba Vì: ~21.07° N, 105.36° E
    base_lat = 21.0700
    base_lon = 105.3620

    xml = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<gpx version="1.1" creator="ESP32-C6 Tracker" xmlns="http://www.topografix.com/GPX/1/1">',
           '  <metadata>',
           '    <name>Ba Vi Summit Trek Demo</name>',
           '  </metadata>',
           '  <trk>',
           '    <name>Ba Vi Summit Trek Demo</name>',
           '    <trkseg>']

    # Tạo đường zig-zag leo núi dốc uốn lượn ~6km
    cur_lat = base_lat
    cur_lon = base_lon
    heading = 45.0  # Đông Bắc

    for i in range(400):
        # Mỗi bước ~15 mét
        heading += math.sin(i * 0.1) * 20.0  # lượn cua
        rad = math.radians(heading)
        cur_lat += (15.0 * math.cos(rad)) / 111320.0
        cur_lon += (15.0 * math.sin(rad)) / (111320.0 * math.cos(math.radians(cur_lat)))
        xml.append(f'      <trkpt lat="{cur_lat:.6f}" lon="{cur_lon:.6f}"><ele>{200 + i * 2.5:.1f}</ele></trkpt>')

    xml.extend(['    </trkseg>', '  </trk>', '</gpx>'])

    with open(demo_path, "w", encoding="utf-8") as f:
        f.write("\n".join(xml))
    print(f"[+] Đã tạo file GPX mẫu: {demo_path}")

def convert_gpx_to_binary(gpx_path, output_path=None, epsilon=5.0):
    if not os.path.isfile(gpx_path):
        print(f"[!] Lỗi: Không tìm thấy file: {gpx_path}")
        return False

    if output_path is None:
        base, _ = os.path.splitext(gpx_path)
        output_path = base + ".bin"

    print(f"\n[*] Đang đọc file GPX: {os.path.basename(gpx_path)}")
    try:
        route_name, raw_points = parse_gpx(gpx_path)
    except Exception as e:
        print(f"[!] Lỗi phân tích GPX: {e}")
        return False

    if not raw_points:
        print("[!] Lỗi: File GPX không chứa điểm tọa độ nào (<trkpt> hoặc <rtept>)!")
        return False

    raw_count = len(raw_points)
    print(f"    - Tên lộ trình : {route_name}")
    print(f"    - Điểm gốc     : {raw_count:,} điểm")

    # Áp dụng nén Ramer-Douglas-Peucker
    if epsilon > 0 and raw_count > 2:
        simplified_points = ramer_douglas_peucker(raw_points, epsilon)
    else:
        simplified_points = raw_points

    simp_count = len(simplified_points)
    saved_pct = (1.0 - simp_count / raw_count) * 100.0 if raw_count > 0 else 0

    print(f"    - Sau khi nén  : {simp_count:,} điểm (Dung sai epsilon = {epsilon}m)")
    print(f"    - Tiết kiệm RAM: Giảm {saved_pct:.1f}% số điểm")

    total_dist_km, min_lat, max_lat, min_lon, max_lon = write_binary_route(
        output_path, route_name, simplified_points
    )

    file_size = os.path.getsize(output_path)
    print(f"\n[✓] XUẤT FILE BINARY THÀNH CÔNG:")
    print(f"    - Đường dẫn    : {output_path}")
    print(f"    - Kích thước   : {file_size:,} bytes (~{file_size / 1024.0:.1f} KB)")
    print(f"    - Quãng đường  : {total_dist_km:.2f} km")
    print(f"    - Vĩ độ Bounds : {min_lat:.6f} -> {max_lat:.6f}")
    print(f"    - Kinh độ Bounds: {min_lon:.6f} -> {max_lon:.6f}")
    print(f"    - RAM ESP32-C6 : Cần đúng 960 bytes RAM đệm (Sliding Window 120 điểm)!")
    return True

def main():
    parser = argparse.ArgumentParser(
        description="ESP32-C6 Outdoor Tracker: GPX to Compact Route Binary Converter",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("gpx_files", nargs="*", help="File .gpx cần chuyển đổi")
    parser.add_argument("-o", "--output", help="Đường dẫn file .bin đầu ra (nếu chuyển 1 file)")
    parser.add_argument("-e", "--epsilon", type=float, default=5.0,
                        help="Dung sai nén Ramer-Douglas-Peucker tính bằng mét (mặc định: 5.0m)")
    parser.add_argument("--demo", action="store_true", help="Tạo file GPX mẫu và xuất ra route demo")

    args = parser.parse_args()

    if args.demo:
        demo_gpx = os.path.join(os.path.dirname(__file__), "demo_bavi_summit.gpx")
        demo_bin = os.path.join(os.path.dirname(__file__), "demo_bavi_summit.bin")
        create_demo_gpx(demo_gpx)
        convert_gpx_to_binary(demo_gpx, demo_bin, epsilon=args.epsilon)
        print("\n[!] Gợi ý: Bạn có thể copy file demo_bavi_summit.bin vào thẻ nhớ SD:")
        print("    /sdcard/routes/active.bin")
        return

    if not args.gpx_files:
        parser.print_help()
        print("\n[!] Không có file GPX nào được chỉ định.")
        print("    Chạy với cờ --demo để tạo file test mẫu:")
        print("    python tools/gpx_to_route.py --demo")
        sys.exit(1)

    for gpx in args.gpx_files:
        out = args.output if len(args.gpx_files) == 1 else None
        convert_gpx_to_binary(gpx, out, epsilon=args.epsilon)

if __name__ == "__main__":
    main()

"""
Generate 3D printable STL files for the ESP32-C6 Outdoor GPS Tracker Enclosure
GARMIN eTREX 32x COMPACT EDITION (86.6 x 47.6 mm):
  1. Garmin eTrex 32x Side Profile & Venting:
     - Deep 45-deg tapered perimeter bevels + 3 smooth 45-deg chamfered horizontal grip channels
     - 3 small TRANSVERSE (across-wall) 45-deg chamfered barometric vent slits (NO stepped ledges!)
  2. Sturdy, Thick-Walled Internal Cradles (ZERO thin/fragile slivers):
     - BMP580 Sensor Bay: Solid U-frame pocket with 2.0mm internal standoff stops (cữ)
       keeping the sensor PCB 2.0mm off the outer wall for optimal airflow & protection
     - ATGM336H GPS PCB Bay: Flush against the opposite inner side wall with thick 2.0mm retaining ribs
     - 20-deg Tilted Cradle for 25x25x4mm Ceramic GPS Antenna (IPEX Gen 1)
  3. Compact Screw Bosses + Thin Li-Po Battery + Quick-Release Bike Mount.
"""

import os
import numpy as np
import manifold3d as m3d
import trimesh
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def rounded_box_2d(w: float, l: float, r: float, segments: int = 56) -> m3d.CrossSection:
    """Create a 2D rounded rectangle centered at (0, 0)."""
    r = max(0.1, min(r, w / 2 - 0.05, l / 2 - 0.05))
    pts = []
    corners = [
        ( w / 2 - r,  l / 2 - r, 0.0),
        (-w / 2 + r,  l / 2 - r, np.pi / 2),
        (-w / 2 + r, -l / 2 + r, np.pi),
        ( w / 2 - r, -l / 2 + r, 3 * np.pi / 2),
    ]
    seg_per_corner = max(6, segments // 4)
    for cx, cy, start_ang in corners:
        for i in range(seg_per_corner):
            ang = start_ang + (np.pi / 2) * (i / seg_per_corner)
            pts.append([cx + r * np.cos(ang), cy + r * np.sin(ang)])
    return m3d.CrossSection([np.array(pts, dtype=np.float32)])


def rounded_box(w: float, l: float, h: float, r: float, center_z: bool = False) -> m3d.Manifold:
    """Extrude a rounded box along Z from 0 to h (or -h/2 to h/2)."""
    cs = rounded_box_2d(w, l, r)
    solid = m3d.Manifold.extrude(cs, h)
    if center_z:
        solid = solid.translate([0, 0, -h / 2])
    return solid


def chamfered_etrex_box(
    w: float,
    l: float,
    h: float,
    r_xy: float,
    chamfer_bot: float = 3.8,
    chamfer_top: float = 0.4,
    segments: int = 64,
) -> m3d.Manifold:
    """Create a Garmin eTrex-style body with deep 45-degree perimeter chamfers."""
    slices = []
    eps = 0.04

    if chamfer_bot > 0.05:
        cs_bot = rounded_box_2d(w - 2.0 * chamfer_bot, l - 2.0 * chamfer_bot, max(0.5, r_xy - chamfer_bot), segments)
        slices.append(m3d.Manifold.extrude(cs_bot, eps))
        cs_mid1 = rounded_box_2d(w, l, r_xy, segments)
        slices.append(m3d.Manifold.extrude(cs_mid1, eps).translate([0, 0, chamfer_bot]))
    else:
        cs_bot = rounded_box_2d(w, l, r_xy, segments)
        slices.append(m3d.Manifold.extrude(cs_bot, eps))

    if chamfer_top > 0.05:
        cs_mid2 = rounded_box_2d(w, l, r_xy, segments)
        slices.append(m3d.Manifold.extrude(cs_mid2, eps).translate([0, 0, h - chamfer_top]))
        cs_top = rounded_box_2d(w - 2.0 * chamfer_top, l - 2.0 * chamfer_top, max(0.5, r_xy - chamfer_top), segments)
        slices.append(m3d.Manifold.extrude(cs_top, eps).translate([0, 0, h - eps]))
    else:
        cs_top = rounded_box_2d(w, l, r_xy, segments)
        slices.append(m3d.Manifold.extrude(cs_top, eps).translate([0, 0, h - eps]))

    return m3d.Manifold.batch_hull(slices)


def chamfered_window_cutter(w: float, l: float, h_total: float, r_xy: float, bevel: float) -> m3d.Manifold:
    """Create a recessed 45-degree chamfered screen bezel window."""
    eps = 0.04
    cs_front = rounded_box_2d(w + 2.0 * bevel, l + 2.0 * bevel, r_xy + bevel, segments=52)
    cs_inner = rounded_box_2d(w, l, r_xy, segments=52)
    chamfer_part = m3d.Manifold.batch_hull([
        m3d.Manifold.extrude(cs_front, eps).translate([0, 0, -0.05]),
        m3d.Manifold.extrude(cs_inner, eps).translate([0, 0, bevel]),
    ])
    straight_part = m3d.Manifold.extrude(cs_inner, h_total).translate([0, 0, bevel - 0.02])
    return chamfer_part + straight_part


def chamfered_vent_slit_x(
    slit_y: float,
    slit_z: float,
    wall_t: float,
    chamfer: float = 0.85,
) -> m3d.Manifold:
    """
    Create a true 45-degree chamfered (non-stepped!) transverse vent slit along the X-axis.
    Inner opening: slit_y x slit_z. Outer opening flares smoothly by +2*chamfer at 45 deg.
    """
    # Build two YZ cross-sections extruded thinly in X and lofted via batch_hull
    eps = 0.04
    cs_in = rounded_box_2d(slit_y, slit_z, min(slit_y, slit_z) * 0.45, segments=24)
    cs_out = rounded_box_2d(
        slit_y + 2.0 * chamfer,
        slit_z + 2.0 * chamfer,
        min(slit_y, slit_z) * 0.45 + chamfer * 0.5,
        segments=24,
    )
    # Extrude along Z then rotate to align extrusion with +X
    s_in  = m3d.Manifold.extrude(cs_in, eps).translate([0, 0, -wall_t * 0.8])
    s_mid = m3d.Manifold.extrude(cs_in, eps).translate([0, 0, -chamfer])
    s_out = m3d.Manifold.extrude(cs_out, eps).translate([0, 0, 0.4])
    funnel = m3d.Manifold.batch_hull([s_in, s_mid]) + m3d.Manifold.batch_hull([s_mid, s_out])
    return funnel.rotate([0, 90, 0])


def cylinder_z(d: float, h: float, fn: int = 36, center_z: bool = False) -> m3d.Manifold:
    cyl = m3d.Manifold.cylinder(h, d / 2, d / 2, fn)
    if center_z:
        cyl = cyl.translate([0, 0, -h / 2])
    return cyl


def box_centered(w: float, l: float, h: float) -> m3d.Manifold:
    return m3d.Manifold.cube([w, l, h], True)


def prism_y(pts_xz: list, length_y: float) -> m3d.Manifold:
    cs = m3d.CrossSection([np.array(pts_xz, dtype=np.float32)])
    solid = m3d.Manifold.extrude(cs, length_y)
    return solid.translate([0, 0, -length_y / 2]).rotate([90, 0, 0])


def prism_x(pts_yz: list, width_x: float) -> m3d.Manifold:
    cs = m3d.CrossSection([np.array(pts_yz, dtype=np.float32)])
    solid = m3d.Manifold.extrude(cs, width_x)
    return solid.translate([0, 0, -width_x / 2]).rotate([90, 0, 90])


# ==============================================================================
# DIMENSIONS (mm) — COMPACT 86.6 x 47.6 mm GARMIN eTREX 32x EDITION
# ==============================================================================
TOL = 0.30
WALL = 2.8

# 1. Waveshare ESP32-C6-Touch-AMOLED-1.64 (with factory frame)
WS_RAW_W = 29.12
WS_RAW_L = 44.00
WS_RAW_H = 9.60

WS_W = WS_RAW_W + TOL * 2     # 29.72 mm
WS_L = WS_RAW_L + TOL * 2     # 44.60 mm
WS_R = 3.5

WIN_W = 26.2
WIN_L = 41.0
BEZEL_T = 1.8

# 2. Compact Outer Dimensions
INNER_W = 42.0                # X: -21.0 .. +21.0 mm
INNER_L = 81.0                # Y: -40.5 .. +40.5 mm
OUTER_W = INNER_W + WALL * 2  # 47.6 mm
OUTER_L = INNER_L + WALL * 2  # 86.6 mm
EXT_R = 10.5                  # Large eTrex corner radius

LIP_H = 1.8
TOP_H = BEZEL_T + WS_RAW_H + LIP_H  # 13.2 mm
MOUNT_BASE_H = 3.6
BOT_FLOOR_Z = MOUNT_BASE_H + 2.0    # 5.6 mm
BOT_H = 17.5

# 3. Internal Zones (Sturdy, Zero Thin Slivers):
#   - Bottom M2.5 Bosses : X = ±11.2, Y = -37.2 mm
#   - Waveshare Screen   : Y = -33.6 .. +11.0 mm (center WS_OFFSET_Y = -11.3 mm)
#   - Top Center (GPS)   : Y = +12.6 .. +36.84 mm (20-deg tilted 25.8x25.8mm antenna, X = -12.9 .. +12.9)
#   - Left Bay (+X)      : BMP580 Sensor Frame (2.0mm standoff from outer wall X=+21.0 -> sensor sits at X=+15.2..+19.0)
#   - Right Bay (-X)     : ATGM336H GPS PCB Bay (flush against outer wall X=-21.0 -> PCB sits at X=-21.0..-16.2)
#   - Top M2.5 Bosses    : X = ±17.8, Y = +31.5 mm
WS_OFFSET_Y = -11.3

GPS_TILT_DEG = 20.0
GPS_ANT_W = 25.8
GPS_ANT_L = 25.8
GPS_START_Y = 12.6

SCREW_BOSSES = [
    (-11.2, -37.2),
    ( 11.2, -37.2),
    (-17.8,  31.5),
    ( 17.8,  31.5),
]


def add_etrex_waist_cut(body: m3d.Manifold, z_start: float, z_end: float) -> m3d.Manifold:
    """Carve subtle Garmin eTrex 32x ergonomic side-waist indent on both flanks."""
    waist_r = 56.0
    indent_depth = 1.0
    waist_y = -11.0
    h = z_end - z_start + 1.0
    for side in [-1, 1]:
        cx = side * (OUTER_W / 2 + waist_r - indent_depth)
        cutter = cylinder_z(waist_r * 2, h, fn=72).translate([cx, waist_y, z_start - 0.5])
        body = body - cutter
    return body


def add_etrex32x_side_grips(body: m3d.Manifold, z_rib_start: float, z_rib_end: float) -> m3d.Manifold:
    """
    Carve Garmin eTrex 32x-style smooth 45-deg chamfered horizontal grip grooves
    creating 4 wide tactile grip pads along the lower side walls.
    """
    rib_h = z_rib_end - z_rib_start + 0.8
    for side in [-1, 1]:
        for gy in [-21.5, -14.5, -7.5]:
            # 45-deg sloped groove profile in (X, Y) so there are no sharp 90-deg edges!
            s = side
            x_out = s * (OUTER_W / 2 + 0.5)
            x_in  = s * (OUTER_W / 2 - 1.15)
            pts_xy = [
                [x_out, gy - 1.8],
                [x_out, gy + 1.8],
                [x_in,  gy + 0.75],
                [x_in,  gy - 0.75],
            ]
            if s < 0:
                pts_xy = pts_xy[::-1]
            cs = m3d.CrossSection([np.array(pts_xy, dtype=np.float32)])
            groove = m3d.Manifold.extrude(cs, rib_h).translate([0, 0, z_rib_start - 0.2])
            body = body - groove
    return body


def build_top_cover() -> m3d.Manifold:
    """Build Garmin eTrex 32x-style Top Cover with sturdy internal sensor & GPS bays."""
    outer = chamfered_etrex_box(
        OUTER_W, OUTER_L, TOP_H, EXT_R, chamfer_bot=3.8, chamfer_top=0.35
    )
    outer = add_etrex_waist_cut(outer, 3.8, TOP_H)

    cavity = rounded_box(INNER_W, INNER_L, TOP_H, EXT_R - WALL).translate([0, 0, BEZEL_T])
    shell = outer - cavity

    rim_recess = rounded_box(
        INNER_W + 1.2, INNER_L + 1.2, LIP_H + 0.2, EXT_R - WALL + 0.6
    ).translate([0, 0, TOP_H - LIP_H])
    shell = shell - rim_recess

    win_cutter = chamfered_window_cutter(
        WIN_W, WIN_L, BEZEL_T + 1.0, r_xy=2.8, bevel=1.4
    ).translate([0, WS_OFFSET_Y, 0])
    shell = shell - win_cutter

    cradle_h = TOP_H - BEZEL_T - LIP_H  # 9.6 mm

    # 1. Waveshare Retaining Cradle (cleanly merged with divider wall, no protruding corner fins!)
    ws_cradle_outer = rounded_box(WS_W + 4.0, WS_L + 3.6, cradle_h, WS_R + 1.6).translate([0, WS_OFFSET_Y, BEZEL_T])

    # 2. 20-deg Tilted Cradle Wedge for 25x25x4mm GPS Antenna
    theta = np.deg2rad(GPS_TILT_DEG)
    proj_y = GPS_ANT_L * np.cos(theta)
    rise_z = GPS_ANT_L * np.sin(theta)
    y0 = GPS_START_Y
    y1 = y0 + proj_y
    z0 = BEZEL_T
    z1 = BEZEL_T + rise_z

    pts_wedge_yz = [
        [y0 - 1.8, BEZEL_T - 0.1],
        [y1 + 0.8, BEZEL_T - 0.1],
        [y1 + 0.8, z1 + 0.8],
        [y0 - 1.8, z0 + 0.8],
    ]
    gps_wedge = prism_x(pts_wedge_yz, GPS_ANT_W + 3.6)

    # 3. STURDY BMP580 SENSOR FRAME (+X side, X = +14.2 .. +21.0, Y = +12.5 .. +27.5 mm)
    # - Outer 2.0mm standoff stops (cữ) at X = +19.0 .. +21.0 mm (top & bottom ends of the sensor bay)
    #   keep the BMP580 PCB exactly 2.0mm away from the 3 vent slits on the outer wall!
    # - Solid inner retaining wall (1.8mm thick at X = +13.6 .. +15.4 mm) + end-stop walls
    #   form a rock-solid slot (X = +15.4 .. +19.0 mm -> 3.6mm wide) to slide the BMP580 board in!
    bmp_standoff_bot = box_centered(2.4, 3.0, cradle_h).translate([20.0, 14.0, BEZEL_T + cradle_h / 2])
    bmp_standoff_top = box_centered(2.4, 3.0, cradle_h).translate([20.0, 26.0, BEZEL_T + cradle_h / 2])
    bmp_inner_rail   = box_centered(2.0, 15.0, cradle_h).translate([14.4, 20.0, BEZEL_T + cradle_h / 2])
    bmp_top_cap      = box_centered(7.6, 2.2, cradle_h).translate([17.6, 27.6, BEZEL_T + cradle_h / 2])

    # 4. STURDY ATGM336H GPS PCB BAY (-X side, flush against outer wall X = -21.0 mm!)
    # - Solid 2.0mm inner retaining rib at X = -14.5 .. -16.5 mm + top end-stop at Y = +27.6 mm
    #   keeps the ATGM336H PCB (15.7 x 13.1 x 4mm) pressed flush & solid against the inner side wall (-21.0 mm)!
    gps_pcb_inner_rib = box_centered(2.2, 15.0, cradle_h).translate([-14.8, 20.0, BEZEL_T + cradle_h / 2])
    gps_pcb_top_cap   = box_centered(7.6, 2.2, cradle_h).translate([-17.6, 27.6, BEZEL_T + cradle_h / 2])

    # 5. 4 M2.5 Screw Bosses
    boss_union = m3d.Manifold()
    for sx, sy in SCREW_BOSSES:
        boss = cylinder_z(5.8, cradle_h).translate([sx, sy, BEZEL_T])
        boss_union = boss_union + boss

    shell = (
        shell
        + ws_cradle_outer
        + gps_wedge
        + bmp_standoff_bot
        + bmp_standoff_top
        + bmp_inner_rail
        + bmp_top_cap
        + gps_pcb_inner_rib
        + gps_pcb_top_cap
        + boss_union
    )

    # --- SUBTRACT POCKETS & CHANNELS ---
    ws_pocket = rounded_box(WS_W, WS_L, TOP_H, WS_R).translate([0, WS_OFFSET_Y, BEZEL_T - 0.05])
    # Clean wire routing notches in the top divider wall (Y = +11.0 .. +12.6) straight into BMP580 & ATGM336H bays
    wire_slot_bmp = box_centered(3.6, 5.0, 5.5).translate([ 17.2, WS_OFFSET_Y + WS_L / 2 + 1.0, BEZEL_T + 7.2])
    wire_slot_gps = box_centered(4.2, 5.0, 5.5).translate([-18.4, WS_OFFSET_Y + WS_L / 2 + 1.0, BEZEL_T + 7.2])
    usb_channel_bot = box_centered(14.5, 8.0, cradle_h).translate([0, WS_OFFSET_Y - WS_L / 2, BEZEL_T + 0.6 + cradle_h / 2])

    ant_pocket = box_centered(GPS_ANT_W, GPS_ANT_L, 14.0).translate([0, GPS_ANT_L / 2, 7.0])
    ant_pocket = ant_pocket.rotate([GPS_TILT_DEG, 0, 0]).translate([0, y0, z0])
    ipex_slot = box_centered(8.0, 6.0, 7.5).translate([-12.5, y0 + 5.0, z0 + 6.5])

    screw_holes = m3d.Manifold()
    for sx, sy in SCREW_BOSSES:
        hole = cylinder_z(2.25, cradle_h).translate([sx, sy, BEZEL_T + 1.2])
        screw_holes = screw_holes + hole

    shell = (
        shell
        - ws_pocket
        - wire_slot_bmp
        - wire_slot_gps
        - usb_channel_bot
        - ant_pocket
        - ipex_slot
        - screw_holes
    )

    # 6. USB Type-C Port Cutout at bottom edge
    usb_cut = rounded_box(12.2, WALL * 4, 6.2, 2.8).translate([0, -OUTER_L / 2, BEZEL_T + 2.4])
    usb_recess = rounded_box(14.4, 3.0, 7.8, 3.4).translate([0, -OUTER_L / 2 - 0.2, BEZEL_T + 1.6])
    shell = shell - usb_cut - usb_recess

    # 7. 3 Small TRANSVERSE 45-deg Chamfered Barometric Vent Slits on BMP580 side (X = +OUTER_W/2)
    # Perpendicular to body length (slit_y=1.1mm, slit_z=4.4mm), smooth 45-deg funnel chamfer (NO steps!)
    for vy in [16.8, 20.0, 23.2]:
        vent = chamfered_vent_slit_x(slit_y=1.1, slit_z=4.4, wall_t=WALL, chamfer=0.85)
        shell = shell - vent.translate([OUTER_W / 2, vy, BEZEL_T + 5.6])

    # 8. Garmin eTrex 32x horizontal grip channels
    shell = add_etrex32x_side_grips(shell, 4.4, TOP_H)

    return shell


def build_bottom_shell() -> m3d.Manifold:
    """Build Garmin eTrex 32x-style Thin Li-Po Bottom Shell."""
    outer = chamfered_etrex_box(
        OUTER_W, OUTER_L, BOT_H, EXT_R, chamfer_bot=4.2, chamfer_top=0.35
    )
    outer = add_etrex_waist_cut(outer, 4.2, BOT_H)

    lip_outer = rounded_box(
        INNER_W + 1.2 - TOL * 2,
        INNER_L + 1.2 - TOL * 2,
        LIP_H,
        EXT_R - WALL + 0.4,
    ).translate([0, 0, BOT_H])

    loop_tab = chamfered_etrex_box(
        18.0, 9.5, 5.5, r_xy=4.2, chamfer_bot=1.2, chamfer_top=1.0
    ).translate([0, OUTER_L / 2 + 1.2, 1.2])
    loop_hole = cylinder_z(5.0, 9.0).translate([0, OUTER_L / 2 + 2.0, 0.0])
    lanyard = loop_tab - loop_hole

    body = outer + lip_outer + lanyard

    cavity = rounded_box(INNER_W, INNER_L, BOT_H + LIP_H + 1.0, EXT_R - WALL).translate([0, 0, BOT_FLOOR_Z])
    body = body - cavity

    # RECESSED QUICK-RELEASE BICYCLE DOVETAIL SOCKET
    rail_len = 40.0
    for side in [-1, 1]:
        s = side
        pts_xz = [
            [s * 9.0,  -0.1],
            [s * 13.6, -0.1],
            [s * 13.6,  3.0],
            [s * 6.5,   3.0],
        ]
        if s < 0:
            pts_xz = pts_xz[::-1]
        groove = prism_y(pts_xz, rail_len).translate([0, -1.0, 0])
        body = body - groove

    entry_pocket = rounded_box(27.2, 8.5, 3.1, 2.0).translate([0, -23.5, -0.1])
    body = body - entry_pocket

    detent_notch = rounded_box(8.8, 4.0, 2.3, 1.2).translate([0, -17.5, -0.2])
    body = body - detent_notch

    # Thin Li-Po Battery Centering Tray (51 x 31 mm, shifted down behind screen)
    bat_w = 31.5
    bat_l = 51.0
    bat_h = 4.5
    bat_y = -8.0
    bat_outer = rounded_box(bat_w + 2.4, bat_l + 2.4, bat_h, 2.5).translate([0, bat_y, BOT_FLOOR_Z])
    bat_inner = rounded_box(bat_w, bat_l, bat_h + 1.0, 1.5).translate([0, bat_y, BOT_FLOOR_Z - 0.1])
    bat_wire_slot = box_centered(16.0, bat_l + 6.0, bat_h + 1.0).translate([0, bat_y, BOT_FLOOR_Z + bat_h / 2])
    bat_side_slot = box_centered(bat_w + 6.0, 24.0, bat_h + 1.0).translate([0, bat_y, BOT_FLOOR_Z + bat_h / 2])
    bat_cradle = bat_outer - bat_inner - bat_wire_slot - bat_side_slot
    body = body + bat_cradle

    # Rear Support Ribs for 20-deg tilted GPS antenna
    pts_rib_yz = [
        [27.0, BOT_FLOOR_Z - 0.1],
        [36.0, BOT_FLOOR_Z - 0.1],
        [36.0, BOT_FLOOR_Z + 6.1],
        [27.0, BOT_FLOOR_Z + 2.8],
    ]
    for rx in [-9.0, 9.0]:
        rib = prism_x(pts_rib_yz, 2.4).translate([rx, 0, 0])
        body = body + rib

    # 4 M2.5 Screw Pillars + Countersunk through-holes
    for sx, sy in SCREW_BOSSES:
        pillar = cylinder_z(5.8, BOT_H - BOT_FLOOR_Z).translate([sx, sy, BOT_FLOOR_Z])
        body = body + pillar
    for sx, sy in SCREW_BOSSES:
        thru_hole = cylinder_z(2.8, BOT_H + LIP_H + 2.0).translate([sx, sy, -0.2])
        csink = cylinder_z(5.2, 3.8).translate([sx, sy, -0.2])
        body = body - thru_hole - csink

    usb_lip_clear = box_centered(14.8, WALL * 4, LIP_H + 2.0).translate([0, -OUTER_L / 2, BOT_H + LIP_H / 2])
    body = body - usb_lip_clear

    for sx, sy in SCREW_BOSSES:
        boss_clear = cylinder_z(6.2, LIP_H + 1.0).translate([sx, sy, BOT_H - 0.1])
        body = body - boss_clear

    # Matching Garmin eTrex 32x horizontal grip channels
    body = add_etrex32x_side_grips(body, 4.8, BOT_H)

    return body


def build_bike_mount_base() -> m3d.Manifold:
    """Build Quick-Release Bicycle Handlebar / Stem Mount Dock."""
    dock_w = 32.0
    dock_l = 48.0
    base_h = 6.5

    base = chamfered_etrex_box(
        dock_w, dock_l, base_h, r_xy=5.5, chamfer_bot=1.2, chamfer_top=0.8
    )
    for sx in [-dock_w / 2 + 1.5, dock_w / 2 - 1.5]:
        for sy in [-11.0, 12.0]:
            ear = cylinder_z(7.6, 4.6).translate([sx, sy, 0.4])
            base = base + ear
            neck_cut = (
                cylinder_z(8.8, 1.5).translate([sx, sy, 2.5])
                - cylinder_z(5.2, 2.0).translate([sx, sy, 2.2])
            )
            base = base - neck_cut

    rail_len = 34.0
    for side in [-1, 1]:
        s = side
        pts_xz = [
            [s * 9.30,  base_h - 1.1],
            [s * 13.20, base_h - 1.1],
            [s * 13.20, base_h + 2.65],
            [s * 6.85,  base_h + 2.65],
        ]
        if s < 0:
            pts_xz = pts_xz[::-1]
        rail = prism_y(pts_xz, rail_len).translate([0, 2.0, 0])
        base = base + rail

    stop_wall = rounded_box(25.0, 4.2, 3.85, 1.6).translate([0, 19.5, base_h - 1.1])
    base = base + stop_wall

    u_slot_left  = box_centered(1.5, 13.5, base_h + 4.0).translate([-4.8, -14.5, base_h / 2])
    u_slot_right = box_centered(1.5, 13.5, base_h + 4.0).translate([ 4.8, -14.5, base_h / 2])
    base = base - u_slot_left - u_slot_right

    latch_bump = rounded_box(7.4, 3.2, 2.8, 1.0).translate([0, -13.5, base_h - 1.1])
    thumb_tab = rounded_box(7.6, 8.5, 3.6, 2.0).translate([0, -23.0, 0.4])
    base = base + latch_bump + thumb_tab

    for sy in [-4.0, 7.0]:
        tunnel_h = box_centered(dock_w + 16.0, 5.0, 2.6).translate([0, sy, 3.1])
        base = base - tunnel_h

    for sx in [-8.5, 8.5]:
        tunnel_v = box_centered(4.6, dock_l + 16.0, 2.2).translate([sx, 0, 2.7])
        base = base - tunnel_v

    v_cut = box_centered(12.0, dock_l + 14.0, 1.6).translate([0, 0, -0.1])
    base = base - v_cut

    return base


def manifold_to_trimesh(m: m3d.Manifold) -> trimesh.Trimesh:
    mesh = m.to_mesh()
    verts = np.array(mesh.vert_properties[:, :3], dtype=np.float64)
    faces = np.array(mesh.tri_verts, dtype=np.int64)
    return trimesh.Trimesh(vertices=verts, faces=faces, process=True)


def render_preview(top_tm: trimesh.Trimesh, bot_tm: trimesh.Trimesh, bike_tm: trimesh.Trimesh, out_png: str):
    fig = plt.figure(figsize=(15, 7.5), dpi=160)
    fig.patch.set_facecolor("#0f172a")

    ax1 = fig.add_subplot(1, 2, 1, projection="3d")
    ax1.set_facecolor("#0f172a")
    tris_top = top_tm.vertices[top_tm.faces]
    poly_top = Poly3DCollection(tris_top, alpha=0.92, facecolor="#22d3ee", edgecolor="#0891b2", linewidths=0.14)
    ax1.add_collection3d(poly_top)
    ax1.set_xlim(-28, 28)
    ax1.set_ylim(-48, 48)
    ax1.set_zlim(0, 26)
    ax1.set_box_aspect([56, 96, 26])
    ax1.view_init(elev=48, azim=-34)
    ax1.set_title("Cận cảnh Nắp trên: Khung BMP580 có cữ 2mm + Khoang GPS sát cạnh + 3 khe gió vát", color="#f8fafc", fontsize=10, pad=10, fontweight="bold")
    ax1.tick_params(colors="#94a3b8", labelsize=8)
    ax1.xaxis.pane.fill = False
    ax1.yaxis.pane.fill = False
    ax1.zaxis.pane.fill = False

    ax2 = fig.add_subplot(1, 2, 2, projection="3d")
    ax2.set_facecolor("#0f172a")
    top_v  = top_tm.vertices  + np.array([-52.0, 0, 0])
    bot_v  = bot_tm.vertices  + np.array([  0.0, 0, 0])
    bike_v = bike_tm.vertices + np.array([ 48.0, 0, 0])
    for verts, faces, color, edge in [
        (top_v,  top_tm.faces,  "#22d3ee", "#0891b2"),
        (bot_v,  bot_tm.faces,  "#34d399", "#059669"),
        (bike_v, bike_tm.faces, "#fbbf24", "#d97706"),
    ]:
        tris = verts[faces]
        poly = Poly3DCollection(tris, alpha=0.90, facecolor=color, edgecolor=edge, linewidths=0.12)
        ax2.add_collection3d(poly)
    ax2.set_xlim(-80, 78)
    ax2.set_ylim(-50, 50)
    ax2.set_zlim(0, 36)
    ax2.set_box_aspect([158, 100, 36])
    ax2.view_init(elev=30, azim=-58)
    ax2.set_title("Toàn bộ 3 chi tiết kiểu Garmin eTrex 32x (Watertight 100%)", color="#f8fafc", fontsize=10, pad=10, fontweight="bold")
    ax2.tick_params(colors="#94a3b8", labelsize=8)
    ax2.xaxis.pane.fill = False
    ax2.yaxis.pane.fill = False
    ax2.zaxis.pane.fill = False

    plt.tight_layout()
    plt.savefig(out_png, dpi=160, facecolor=fig.get_facecolor())
    plt.close(fig)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    top_m  = build_top_cover()
    bot_m  = build_bottom_shell()
    bike_m = build_bike_mount_base()

    plate_m = (
        top_m.translate([-52.0, 0, 0])
        + bot_m.translate([0.0, 0, 0])
        + bike_m.translate([48.0, 0, 0])
    )

    top_tm   = manifold_to_trimesh(top_m)
    bot_tm   = manifold_to_trimesh(bot_m)
    bike_tm  = manifold_to_trimesh(bike_m)
    plate_tm = manifold_to_trimesh(plate_m)

    top_path   = os.path.join(out_dir, "tracker_top_cover.stl")
    bot_path   = os.path.join(out_dir, "tracker_bottom_shell.stl")
    bike_path  = os.path.join(out_dir, "tracker_bike_mount_base.stl")
    plate_path = os.path.join(out_dir, "tracker_full_print_plate.stl")
    png_path   = os.path.join(out_dir, "preview_3d_case.png")

    top_tm.export(top_path)
    bot_tm.export(bot_path)
    bike_tm.export(bike_path)
    plate_tm.export(plate_path)
    render_preview(top_tm, bot_tm, bike_tm, png_path)

    print(f"Top Cover STL     : {top_path} (watertight={top_tm.is_watertight}, extents={np.round(top_tm.extents, 2)} mm)")
    print(f"Bottom Shell STL  : {bot_path} (watertight={bot_tm.is_watertight}, extents={np.round(bot_tm.extents, 2)} mm)")
    print(f"Bike Mount STL    : {bike_path} (watertight={bike_tm.is_watertight}, extents={np.round(bike_tm.extents, 2)} mm)")
    print(f"Full Plate STL    : {plate_path} (watertight={plate_tm.is_watertight}, extents={np.round(plate_tm.extents, 2)} mm)")
    print(f"Preview Image     : {png_path}")


if __name__ == "__main__":
    main()

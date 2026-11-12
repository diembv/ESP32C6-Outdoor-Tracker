// ============================================================================
// ESP32-C6 OUTDOOR GPS TRACKER — 3D PRINTABLE ENCLOSURE (OpenSCAD Parametric)
// Thiết kế dành riêng cho:
//   1. Waveshare ESP32-C6-Touch-AMOLED-1.64 (Bản có vỏ hãng: 44.00 x 29.12 x 9.60 mm)
//   2. Module GPS ATGM336H + Anten gốm (16x14mm PCB + 25x25mm / 18x18mm Patch Antenna)
//   3. Cảm biến áp suất khí quyển BMP580 (kèm khe thoáng khí cân bằng áp suất)
//   4. Khoang Pin Li-Po 3.7V rộng rãi (52 x 32 x 11 mm, hỗ trợ pin 500mAh - 1500mAh)
// ============================================================================

$fn = 48;

// --- THÔNG SỐ DUNG SAI MÁY IN 3D (FDM 0.4mm Nozzle) ---
tol = 0.30;               // Dung sai lắp ghép chuẩn (mm)
wall = 2.4;               // Độ dày thành vỏ (6 lines với nozzle 0.4mm -> rất cứng cáp)

// --- 1. KÍCH THƯỚC MODULE MÀN HÌNH WAVESHARE 1.64" (CÓ VỎ HÃNG) ---
ws_w = 29.12 + tol*2;     // Rộng khoang chứa: ~29.72 mm
ws_l = 44.00 + tol*2;     // Dài khoang chứa:  ~44.60 mm
ws_h = 9.60;              // Độ dày module Waveshare: 9.60 mm
ws_r = 3.5;               // Bán kính bo góc vỏ Waveshare

// Cửa sổ hiển thị & vuốt cảm ứng mặt trước (giữ lại gờ 1.4mm quanh viền vỏ)
win_w = 26.2;             // Vùng hiển thị thực 22.34mm -> mở 26.2mm thao tác cảm ứng thoải mái
win_l = 41.0;             // Vùng hiển thị thực 36.06mm -> mở 41.0mm
bezel_t = 1.4;            // Độ dày gờ chặn mặt trước

// --- 2. KÍCH THƯỚC TỔNG THỂ HỘP OUTDOOR TRACKER ---
// Bố cục kiểu máy định vị Garmin cầm tay:
// - Phần dưới: Màn hình Waveshare 1.64" (dọc) + Khoang BMP580 bên cạnh
// - Phần đỉnh trên: Khoang Anten gốm GPS hướng lên trời (thu sóng tốt nhất)
// - Phần đáy dưới: Khoang Pin Li-Po (52 x 32 x 11 mm) nằm dưới vách ngăn
inner_w = 36.0;           // Rộng lòng hộp: 36 mm (vừa pin 30-32mm & màn hình 29.7mm)
inner_l = 74.0;           // Dài lòng hộp:  74 mm (Màn hình 45mm + Khoang GPS/BMP580 26mm)
top_h   = 11.5;           // Chiều cao nắp trên (ôm trọn module Waveshare 9.6mm + gờ 1.4mm)
bot_h   = 14.5;           // Chiều cao thân dưới (chứa pin dày 10-11mm + dây nối)
ext_r   = 5.0;            // Bo góc ngoài hộp

outer_w = inner_w + wall*2; // 40.8 mm
outer_l = inner_l + wall*2; // 78.8 mm

// Tọa độ tâm màn hình Waveshare (dịch về phía dưới để nhường đỉnh cho Anten GPS)
ws_offset_y = -11.5;

// --- MODULE BO GÓC HÌNH HỘP 2D/3D ---
module rounded_box(w, l, h, r) {
    hull() {
        for (x = [-w/2 + r, w/2 - r])
            for (y = [-l/2 + r, l/2 - r])
                translate([x, y, 0])
                    cylinder(r = r, h = h);
    }
}

// --- PHẦN 1: NẮP TRÊN (TOP COVER - ÔM MÀN HÌNH + GPS + BMP580) ---
// In úp mặt phẳng xuống bàn in (không cần support!)
module top_cover() {
    difference() {
        union() {
            // Vỏ ngoài nắp trên
            rounded_box(outer_w, outer_l, top_h, ext_r);
        }

        // Khoét rỗng lòng chung nắp trên (để lại mặt tiền dày bezel_t = 1.4mm)
        translate([0, 0, bezel_t])
            rounded_box(inner_w, inner_l, top_h, ext_r - wall);

        // 1. Cửa sổ màn hình cảm ứng AMOLED 1.64" (vát mép 45 độ dễ vuốt cạnh)
        translate([0, ws_offset_y, -0.1])
            rounded_box(win_w, win_l, bezel_t + 0.3, 2.5);

        // 2. Khe cắm cáp USB Type-C ở cạnh đáy (thẳng tâm màn hình Waveshare)
        translate([0, -outer_l/2 + wall/2, bezel_t + 4.5])
            rotate([90, 0, 0])
            hull() {
                translate([-3.5, 0, -5]) cylinder(r = 3.2, h = 10);
                translate([ 3.5, 0, -5]) cylinder(r = 3.2, h = 10);
            }

        // 3. Cửa sổ bấm nút BOOT & PWR bên hông phải của vỏ Waveshare
        translate([outer_w/2 - wall/2, ws_offset_y + 2.0, bezel_t + 2.2])
            cube([wall + 4, 18.0, 5.5], center = true);

        // 4. Khe thoáng khí cho cảm biến áp suất & nhiệt độ BMP580 (bên hông trái phía trên)
        for (i = [-1:1]) {
            translate([-outer_w/2, 22.0 + i*3.5, top_h/2 + 1.0])
                cube([wall*3, 1.6, 5.0], center = true);
        }

        // 5. Lỗ bắt ốc M2.5 ở 4 góc
        for (x = [-inner_w/2 + 2.8, inner_w/2 - 2.8])
            for (y = [-inner_l/2 + 2.8, inner_l/2 - 2.8])
                translate([x, y, 2.0])
                    cylinder(d = 2.3, h = top_h);
    }

    // Khung định vị (Cradle) giữ chặt vỏ Waveshare 44.0 x 29.12 mm không xê dịch
    translate([0, ws_offset_y, bezel_t]) {
        difference() {
            rounded_box(ws_w + 3.2, ws_l + 3.2, ws_h, ws_r + 1.2);
            translate([0, 0, -0.1])
                rounded_box(ws_w, ws_l, ws_h + 0.3, ws_r);
            // Cắt lối đi dây phía trên (GPS/I2C/Pin) & khe USB-C phía dưới
            translate([0, ws_l/2, ws_h/2])
                cube([18, 8, ws_h + 1], center = true);
            translate([0, -ws_l/2, ws_h/2])
                cube([14, 8, ws_h + 1], center = true);
            // Cắt hở hông phải cho nút bấm BOOT / PWR
            translate([ws_w/2, 2.0, ws_h/2])
                cube([8, 20, ws_h + 1], center = true);
        }
    }

    // Khung giữ Anten gốm GPS (25.5 x 25.5 mm) ở đỉnh trên của nắp
    translate([0, 21.5, bezel_t]) {
        difference() {
            cube([28.5, 27.5, 6.0], center = false);
        }
    }

    // 4 trụ bắt vít M2.5 ở 4 góc trong lòng nắp trên
    for (x = [-inner_w/2 + 2.8, inner_w/2 - 2.8])
        for (y = [-inner_l/2 + 2.8, inner_l/2 - 2.8])
            translate([x, y, bezel_t]) {
                difference() {
                    cylinder(d = 5.6, h = top_h - bezel_t);
                    translate([0, 0, 1.0])
                        cylinder(d = 2.2, h = top_h);
                }
            }
}

// --- PHẦN 2: THÂN DƯỚI (BOTTOM SHELL - KHOANG PIN LI-PO + MÓC ĐEO DÂY) ---
module bottom_shell() {
    difference() {
        union() {
            // Thân hộp dưới
            rounded_box(outer_w, outer_l, bot_h, ext_r);

            // Gờ âm dương (Lip) cao 1.8mm giúp khớp khít với nắp trên
            translate([0, 0, bot_h])
                rounded_box(inner_w - tol*2, inner_l - tol*2, 1.8, ext_r - wall);

            // Quai đeo dây (Lanyard / Carabiner Loop) ở đỉnh trên
            translate([0, outer_l/2 + 2.5, 0]) {
                difference() {
                    hull() {
                        translate([0, -2.5, 0]) cube([18, 2, 5.0], center = true);
                        translate([0, 1.5, 2.5]) cylinder(r = 5.0, h = 5.0, center = true);
                    }
                    translate([0, 1.2, 2.5])
                        cylinder(d = 5.5, h = 10, center = true);
                }
            }
        }

        // Khoét lòng khoang chứa Pin Li-Po (52 x 32 x 11 mm dư sức vừa trong lòng 36 x 74 mm)
        translate([0, 0, wall])
            rounded_box(inner_w, inner_l, bot_h + 3.0, ext_r - wall);

        // Khoét lối cáp USB-C ở cạnh đáy
        translate([0, -outer_l/2, bot_h])
            cube([13.0, wall*3, 5.0], center = true);

        // Khoét hở cửa sổ nút bấm bên hông
        translate([outer_w/2, ws_offset_y + 2.0, bot_h])
            cube([wall*3, 18.0, 4.5], center = true);

        // 4 lỗ ốc M2.5 xuyên từ mặt đáy (có bậc âm đầu ốc M2.5)
        for (x = [-inner_w/2 + 2.8, inner_w/2 - 2.8])
            for (y = [-inner_l/2 + 2.8, inner_l/2 - 2.8]) {
                translate([x, y, -0.1])
                    cylinder(d = 2.8, h = bot_h + 5);
                translate([x, y, -0.1])
                    cylinder(d = 5.0, h = 2.0);
            }
    }

    // 4 trụ đỡ góc + giữ cố định viên pin Li-Po ở giữa (~52 x 32 mm)
    for (x = [-inner_w/2 + 2.8, inner_w/2 - 2.8])
        for (y = [-inner_l/2 + 2.8, inner_l/2 - 2.8])
            translate([x, y, wall]) {
                difference() {
                    cylinder(d = 6.0, h = bot_h - wall);
                    translate([0, 0, -0.1])
                        cylinder(d = 2.8, h = bot_h);
                }
            }
}

// --- HIỂN THỊ CẢ 2 PHẦN ĐỂ IN CÙNG LÚC ---
translate([-outer_w/2 - 5, 0, 0]) top_cover();
translate([ outer_w/2 + 5, 0, 0]) bottom_shell();

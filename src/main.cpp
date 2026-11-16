/**
 * @file    main.cpp
 * @brief   ESP32 Outdoor Tracker — Multi-Board HAL Architecture
 *
 * Default Board : Waveshare ESP32-C6-Touch-AMOLED-1.64
 * Screen        : SH8601 AMOLED 280 x 456 px (QSPI)
 *
 * All I2C sensors & PMICs share the single-owner Arduino Wire bus via hal_i2c
 * to avoid ESP-IDF driver_ng vs legacy Wire conflicts.
 */

#include <Arduino.h>
#include <math.h>
#include <lvgl.h>
#include "boards/board_select.h"
#include "app_types.h"
#include "hal/hal_i2c.h"
#include "hal/hal_power.h"
#include "hal/hal_sensors.h"
#include "hal/hal_display.h"
#include "ui_dashboard.h"
#include "core/baro_fusion.h"
#include "core/track_logger.h"
#include "route_nav.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  Timing
// ═══════════════════════════════════════════════════════════════════════════════
static constexpr uint32_t SENSOR_READ_MS  = 200;  // 5Hz
static constexpr uint32_t UI_UPDATE_MS    = 500;  // 2Hz (on dinh bus, khong giat lag)
static constexpr uint32_t SERIAL_PRINT_MS = 2000;
static constexpr uint32_t GPS_TIMEOUT_MS  = 12000;
static constexpr uint32_t I2C_RETRY_EVERY = 10;

// ═══════════════════════════════════════════════════════════════════════════════
//  State
// ═══════════════════════════════════════════════════════════════════════════════
static uint32_t tLastSensor  = 0;
static uint32_t tLastUiUpd   = 0;
static uint32_t tLastPrint   = 0;
static uint32_t tLastGps     = 0;
static uint32_t tBoot        = 0;
static uint8_t  printCount   = 0;

SensorSnapshot g_snap = {};

// ═══════════════════════════════════════════════════════════════════════════════
//  Serial print (dùng integer split, không dùng %f)
// ═══════════════════════════════════════════════════════════════════════════════
static void printSerial() {
    Serial.println(F("\n=================================================="));
    Serial.println(F("  [GPS] GNSS"));
    if (g_snap.gps.fix_valid) {
        double aLat = g_snap.gps.latitude  >= 0 ? g_snap.gps.latitude  : -g_snap.gps.latitude;
        double aLon = g_snap.gps.longitude >= 0 ? g_snap.gps.longitude : -g_snap.gps.longitude;
        long   lI   = (long)aLat, lonI = (long)aLon;
        long   lD   = (long)((aLat-lI)*1000000), lonD = (long)((aLon-lonI)*1000000);
        Serial.printf("    Lat/Lon: %ld.%06ld%c / %ld.%06ld%c\n",
            lI, lD, g_snap.gps.latitude  >= 0 ? 'N' : 'S',
            lonI, lonD, g_snap.gps.longitude >= 0 ? 'E' : 'W');
    } else {
        Serial.println(F("    Vị trí : Chưa fix..."));
    }
    {
        long sI = (long)g_snap.gps.speed_kmh;
        long sD = (long)((g_snap.gps.speed_kmh-sI)*10);
        long hdI = (long)g_snap.gps.hdop;
        long hdD = (long)((g_snap.gps.hdop - hdI) * 100);
        Serial.printf("    Speed  : %ld.%01ld km/h  Sats: %lu (HDOP: %ld.%02ld)  Course: %ld deg\n",
            sI, sD, (unsigned long)g_snap.gps.satellites,
            hdI, labs(hdD), (long)g_snap.gps.course_deg);
        Serial.printf("    NMEA   : chars=%lu  fixes=%lu  err=%lu\n",
            (unsigned long)g_snap.gps.chars_proc,
            (unsigned long)g_snap.gps.fixes,
            (unsigned long)g_snap.gps.checksum_err);
    }

    Serial.println(F("  [BMP] BMP580"));
    if (g_snap.baro.valid) {
        long tI = (long)g_snap.baro.temperature_c;
        long tD = (long)((g_snap.baro.temperature_c >= 0 ?
                   g_snap.baro.temperature_c-tI : -g_snap.baro.temperature_c+tI)*100);
        long pI = (long)g_snap.baro.pressure_hpa;
        long pD = (long)((g_snap.baro.pressure_hpa-pI)*100);
        long aI = (long)g_snap.baro.altitude_m;
        long aD = (long)((g_snap.baro.altitude_m >= 0 ?
                   g_snap.baro.altitude_m-aI : -g_snap.baro.altitude_m+aI)*10);
        Serial.printf("    T: %ld.%02ld C  P: %ld.%02ld hPa  Alt: %ld.%01ld m %s\n",
            tI, labs(tD), pI, labs(pD), aI, labs(aD),
            g_snap.baro.calibrated ? "(DA CALIBRATE)" : "(CHUA CALIBRATE, P0=1013.25)");
    } else {
        Serial.println(F("    Sensor chưa sẵn sàng."));
    }

    Serial.println(F("  [IMU] QMI8658 (hal_i2c)"));
    if (g_snap.imu.valid) {
        long piI = (long)g_snap.imu.pitch_deg;
        long piD = (long)((g_snap.imu.pitch_deg >= 0 ?
                    g_snap.imu.pitch_deg-piI : -g_snap.imu.pitch_deg+piI)*10);
        long riI = (long)g_snap.imu.roll_deg;
        long riD = (long)((g_snap.imu.roll_deg >= 0 ?
                    g_snap.imu.roll_deg-riI : -g_snap.imu.roll_deg+riI)*10);
        Serial.printf("    Pitch: %ld.%01ld deg  Roll: %ld.%01ld deg\n",
            piI, labs(piD), riI, labs(riD));
    } else {
        Serial.println(F("    IMU offline."));
    }

    uint32_t h = g_snap.sys.uptime_s/3600;
    uint32_t m = (g_snap.sys.uptime_s%3600)/60;
    uint32_t s = g_snap.sys.uptime_s%60;
    Serial.printf("  [SYS] Uptime: %02lu:%02lu:%02lu\n",
        (unsigned long)h, (unsigned long)m, (unsigned long)s);
    if (g_snap.sys.sd_ok) {
        Serial.printf("  [SYS] SD Card: OK (%lu MB) - logging\n", (unsigned long)g_snap.sys.sd_free_mb);
    } else {
        if (g_snap.sys.sd_free_mb > 0) {
            Serial.printf("  [SYS] SD Card: ERROR 0x%X\n", (unsigned int)g_snap.sys.sd_free_mb);
        } else {
            Serial.println("  [SYS] SD Card: No SD Card");
        }
    }
    Serial.println(F("==================================================\n"));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // FIX: Tranh treo khi rut cap USB CDC!
    delay(2000);
    tBoot = millis();

    // --- Chan doan reset ---
    hal_power_track_boot(&g_snap.sys.boot_count, &g_snap.sys.reset_reason);
    Serial.printf("[SYS] Boot #%lu, reset reason: %s\n",
                  (unsigned long)g_snap.sys.boot_count, g_snap.sys.reset_reason);

    Serial.println(F("\n╔══════════════════════════════════════════════════╗"));
    Serial.println(F("║  Outdoor Tracker — Multi-Board HAL              ║"));
    Serial.println(F("║  Power & Storage (SD Logger Optimized)          ║"));
    Serial.println(F("╚══════════════════════════════════════════════════╝\n"));
    Serial.flush();

    Serial.printf("[SYS] Board: %s\n", BOARD_NAME);

    // Vô hiệu hóa thẻ SD ngay từ đầu bằng cách kéo chân CS lên HIGH
    // Đảm bảo thẻ SD không làm nhiễu bus SPI (đặc biệt là tín hiệu MISO) khi AMOLED khởi tạo
#if defined(BOARD_SD_MODE_SDSPI) && BOARD_SD_MODE_SDSPI
    pinMode(BOARD_SD_CS_PIN, OUTPUT);
    digitalWrite(BOARD_SD_CS_PIN, HIGH);
#endif

    baro_fusion_init();

    // ── I2C + Power + Sensors ─────────────────────────────────────────────────
    hal_i2c_init(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN, BOARD_I2C_FREQ_HZ);
    hal_power_init();
    hal_i2c_scan();
    Serial.flush();

    hal_sensors_init();
    tLastGps = millis();
    Serial.flush();

    // ── Display + LVGL (Unified QSPI HAL) ────────────────────────────────────
    Serial.println(F("[SYS] Khởi tạo AMOLED + LVGL..."));
    hal_display_init();

    // ── Build Dashboard UI ────────────────────────────────────────────────────
    if (hal_display_lock(-1)) {
        ui_dashboard_init();
        hal_display_unlock();
    }
    Serial.println(F("[SYS] Dashboard OK — swipe để chuyển trang.\n"));
    Serial.flush();

    // ── MicroSD Logger & Route Nav ────────────────────────────────────────────
    if (hal_display_lock(-1)) {
        track_logger_init(&g_snap);
        route_nav_init();
        if (g_snap.sys.sd_ok) {
            route_nav_autoload();
        }
        hal_display_set_brightness(180);
        hal_display_unlock();
    }
    Serial.flush();

    tLastSensor = millis();
    tLastUiUpd  = millis();
    tLastPrint  = millis();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  loop()
// ═══════════════════════════════════════════════════════════════════════════════
void loop() {
    const uint32_t now = millis();
    static uint32_t lpt=0;
    if(now-lpt>1000){ lpt=now; Serial.println("[DEBUG] Loop is running!"); }

    // ── Quản lý tắt/bật màn hình (Mặc định LUÔN SÁNG, chỉ tắt khi bấm nút BOOT) ──
    static bool isScreenOn = true;
    static float lastPitch = 0.0f, lastRoll = 0.0f;

    // ── 1. Nút BOOT (GPIO9): Nhấn giữ > 1200ms để bật/tắt màn hình (Chống nhiễu tuyệt đối) ──
    static uint32_t btn_press_start = 0;
    static bool btn_handled = false;
    bool btn_down = hal_power_is_button_pressed();

    if (btn_down) {
        if (btn_press_start == 0) {
            btn_press_start = now;
        } else if (!btn_handled && (now - btn_press_start >= 1200)) {
            btn_handled = true;
            if (hal_display_lock(-1)) {
                isScreenOn = !isScreenOn;
                hal_display_set_brightness(isScreenOn ? 180 : 0);
                if (isScreenOn) {
                    lv_disp_trig_activity(NULL);
                }
                hal_display_unlock();
                Serial.printf("[PWR] Giữ nút BOOT > 1200ms -> Màn hình: %s\n", isScreenOn ? "BAT" : "TAT");
            }
        }
    } else {
        btn_press_start = 0;
        btn_handled = false;
    }

    // ── 2. Khi màn hình đang tắt: Chạm vào màn hình hoặc chuyển động để bật lại ──
    if (!isScreenOn) {
        static uint32_t last_touch_wake_check = 0;
        if (now - last_touch_wake_check >= 100) {
            last_touch_wake_check = now;
            if (hal_display_lock(-1)) {
                uint32_t inactive_ms = lv_disp_get_inactive_time(NULL);
                if (inactive_ms < 150) {
                    isScreenOn = true;
                    hal_display_set_brightness(180);
                    Serial.println(F("[PWR] Chạm màn hình -> Bật sáng lại!"));
                }
                hal_display_unlock();
            }
        }

        // Lắc tay / Chuyển động (Wake-on-motion)
        if (g_snap.imu.valid) {
            if (abs(g_snap.imu.pitch_deg - lastPitch) > 25.0f ||
                abs(g_snap.imu.roll_deg - lastRoll) > 25.0f) {
                if (hal_display_lock(-1)) {
                    isScreenOn = true;
                    hal_display_set_brightness(180);
                    lv_disp_trig_activity(NULL);
                    hal_display_unlock();
                    Serial.println(F("[PWR] Chuyển động mạnh -> Bật sáng màn hình!"));
                }
            }
            lastPitch = g_snap.imu.pitch_deg;
            lastRoll  = g_snap.imu.roll_deg;
        }
    }

    // ── Đọc Pin (PMIC AXP2101 hoặc ADC, Mỗi 2 giây) ──────────────────────────
    static uint32_t lastBat = 0;
    if (now - lastBat >= 2000) {
        lastBat = now;
        hal_power_read_battery(&g_snap.sys.battery_v, &g_snap.sys.battery_pct);
    }

    // ── 4. Đọc dữ liệu GPS NMEA liên tục ─────────────────────────────────────
    if (hal_sensors_poll_gps()) {
        tLastGps = now;
    }

    // Cảnh báo GPS timeout
    if (now - tLastGps > GPS_TIMEOUT_MS) {
        Serial.println(F("[GPS] ⚠️ Không nhận dữ liệu > 12s!"));
        tLastGps = now;
    }

    // ── 5. Đọc sensor mỗi 200ms (fast) / 1000ms (slow) & cập nhật lộ trình ───
    if (now - tLastSensor >= SENSOR_READ_MS) {
        tLastSensor = now;
        static uint32_t lastSlowRead = 0;
        bool doSlowRead = (now - lastSlowRead >= 1000) || (lastSlowRead == 0);

        if (hal_display_lock(-1)) {
            hal_sensors_read_fast(&g_snap);
            if (doSlowRead) {
                lastSlowRead = now;
                hal_sensors_read_slow(now, tBoot, &g_snap);
            }
            if (g_snap.gps.fix_valid) {
                route_nav_update(g_snap.gps.latitude, g_snap.gps.longitude, g_snap.gps.speed_kmh);
            }
            hal_display_unlock();
        }
    }

    // ── 6. Update LVGL UI mỗi 500ms (chỉ khi màn hình đang bật) ──────────────
    if (isScreenOn && (now - tLastUiUpd >= UI_UPDATE_MS)) {
        tLastUiUpd = now;
        if (hal_display_lock(-1)) {
            ui_dashboard_update(&g_snap);
            hal_display_unlock();
        }
    }

    // ── 7. Serial debug mỗi 2s ───────────────────────────────────────────────
    if (now - tLastPrint >= SERIAL_PRINT_MS) {
        tLastPrint = now;
        if (printCount++ % I2C_RETRY_EVERY == 0) {
            if (hal_display_lock(-1)) {
                hal_sensors_retry_i2c();
                hal_display_unlock();
            }
        }
        printSerial();
    }

    // ── 8. Ghi log thẻ nhớ định kỳ (mỗi 1 giây) ──────────────────────────────
    // Khóa LVGL nếu SD card dùng chung bus SPI2 với màn hình AMOLED
    static uint32_t tLastSdLog = 0;
    if (now - tLastSdLog >= 1000) {
        tLastSdLog = now;
        if (track_logger_is_ok()) {
#if defined(BOARD_SD_SHARES_DISPLAY_BUS) && BOARD_SD_SHARES_DISPLAY_BUS
            if (hal_display_lock(-1)) {
                track_logger_log(&g_snap, now);
                hal_display_unlock();
            }
#else
            track_logger_log(&g_snap, now);
#endif
        }
    }

    // ── 9. Nhường CPU cho FreeRTOS task khác ─────────────────────────────────
    delay(5);
}

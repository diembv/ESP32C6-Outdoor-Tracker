/**
 * @file hal_sensors_esp32.cpp
 * @brief ESP32 sensor backend (ATGM336H/LC76G GNSS, BMP580, QMI8658, PCF85063 RTC).
 */

#include "hal/hal_sensors.h"
#include "hal/hal_i2c.h"
#include "hal/common/qmi8658.h"
#include "hal/common/pcf85063.h"
#include "core/baro_fusion.h"
#include "boards/board_select.h"

#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BMP5xx.h>
#include <time.h>
#include <sys/time.h>

static HardwareSerial  s_gps_serial(BOARD_GPS_UART_NUM);
static TinyGPSPlus     s_gps_parser;
static Adafruit_BMP5xx s_bmp;

static bool s_bmp_ok      = false;
static bool s_qmi_ok      = false;
static bool s_time_synced = false;

static bool init_bmp580(void) {
    Serial.printf("[BMP] Ket noi 0x%02X ... ", BOARD_BMP580_I2C_ADDR);
    bool ok = false;
    if (hal_i2c_lock(200)) {
        ok = s_bmp.begin(BOARD_BMP580_I2C_ADDR, &Wire);
        if (ok) {
            s_bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_8X);
            s_bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_4X);
            s_bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
            s_bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
        }
        hal_i2c_unlock();
    }
    Serial.println(ok ? F("OK") : F("THAT BAI."));
    return ok;
}

void hal_sensors_init(void) {
    // 1. Hardware RTC (PCF85063 on S3 boards)
#if defined(BOARD_HAS_RTC_PCF85063) && BOARD_HAS_RTC_PCF85063
    if (pcf85063_init()) {
        if (pcf85063_sync_system_clock()) {
            Serial.println(F("[RTC] PCF85063 OK -> Da dong bo gio he thong."));
        } else {
            Serial.println(F("[RTC] PCF85063 OK (chua co gio chuan, cho GPS)."));
        }
    } else {
        Serial.println(F("[RTC] PCF85063 offline."));
    }
#endif

    // 2. Barometer (BMP580 - optional external I2C)
    s_bmp_ok = init_bmp580();

    // 3. 6-axis IMU (QMI8658)
#if defined(BOARD_HAS_IMU_QMI8658) && BOARD_HAS_IMU_QMI8658
    s_qmi_ok = qmi8658_init();
#endif

    // 4. GNSS UART
#if defined(BOARD_GPS_RX_PIN) && (BOARD_GPS_RX_PIN >= 0)
    Serial.printf("[GPS] Init UART%d: RX=GPIO%d  TX=GPIO%d  %d baud\n",
                  BOARD_GPS_UART_NUM, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN, BOARD_GPS_BAUD);
    s_gps_serial.begin(BOARD_GPS_BAUD, SERIAL_8N1, BOARD_GPS_RX_PIN, BOARD_GPS_TX_PIN);
    Serial.println(F("[GPS] Dang cho NMEA data...\n"));
#endif
}

bool hal_sensors_poll_gps(void) {
    bool updated = false;
    while (s_gps_serial.available()) {
        if (s_gps_parser.encode((char)s_gps_serial.read())) {
            updated = true;
        }
    }
    return updated;
}

void hal_sensors_read_fast(SensorSnapshot *snap) {
    if (!snap) return;
    if (s_qmi_ok) {
        float pitch = 0.0f, roll = 0.0f;
        if (qmi8658_read_tilt(&pitch, &roll)) {
            snap->imu.pitch_deg = pitch;
            snap->imu.roll_deg  = roll;
            snap->imu.valid     = true;
            return;
        }
    }
    snap->imu.valid = false;
}

void hal_sensors_read_slow(uint32_t now_ms, uint32_t boot_ms, SensorSnapshot *snap) {
    if (!snap) return;

    // ── GPS ───────────────────────────────────────────────────────────────────
    snap->gps.fix_valid      = s_gps_parser.location.isValid();
    snap->gps.satellites     = s_gps_parser.satellites.isValid() ? s_gps_parser.satellites.value() : 0;
    snap->gps.speed_kmh      = s_gps_parser.speed.isValid() ? s_gps_parser.speed.kmph() : 0.0f;
    snap->gps.altitude_valid = s_gps_parser.altitude.isValid();
    snap->gps.altitude_m     = snap->gps.altitude_valid ? s_gps_parser.altitude.meters() : 0.0f;
    snap->gps.hdop           = s_gps_parser.hdop.isValid() ? (float)s_gps_parser.hdop.hdop() : 99.9f;
    snap->gps.course_deg     = s_gps_parser.course.isValid() ? (float)s_gps_parser.course.deg() : 0.0f;
    snap->gps.chars_proc     = s_gps_parser.charsProcessed();
    snap->gps.fixes          = s_gps_parser.sentencesWithFix();
    snap->gps.checksum_err   = s_gps_parser.failedChecksum();
    if (snap->gps.fix_valid) {
        snap->gps.latitude  = s_gps_parser.location.lat();
        snap->gps.longitude = s_gps_parser.location.lng();
    }

    // ── GPS Date & Time (UTC) ─────────────────────────────────────────────────
    snap->gps.date_valid = s_gps_parser.date.isValid();
    if (snap->gps.date_valid) {
        snap->gps.year  = s_gps_parser.date.year();
        snap->gps.month = s_gps_parser.date.month();
        snap->gps.day   = s_gps_parser.date.day();
    }

    snap->gps.time_valid = s_gps_parser.time.isValid();
    if (snap->gps.time_valid) {
        snap->gps.hour   = s_gps_parser.time.hour();
        snap->gps.minute = s_gps_parser.time.minute();
        snap->gps.second = s_gps_parser.time.second();
    }

    if (!s_time_synced && snap->gps.date_valid && snap->gps.time_valid && snap->gps.year >= 2023) {
        struct tm t = {};
        t.tm_year  = snap->gps.year - 1900;
        t.tm_mon   = snap->gps.month - 1;
        t.tm_mday  = snap->gps.day;
        t.tm_hour  = snap->gps.hour;
        t.tm_min   = snap->gps.minute;
        t.tm_sec   = snap->gps.second;
        t.tm_isdst = 0;

        time_t epoch = mktime(&t);
        if (epoch != -1) {
            struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            s_time_synced = true;
            Serial.println(F("[RTC] Dong ho he thong da duoc dong bo voi GPS (UTC)."));
#if defined(BOARD_HAS_RTC_PCF85063) && BOARD_HAS_RTC_PCF85063
            if (pcf85063_is_ready()) {
                gmtime_r(&epoch, &t);
                pcf85063_set_time(&t);
                Serial.println(F("[RTC] Da luu gio chuan GPS vao PCF85063."));
            }
#endif
        }
    }

    // ── BMP580 ────────────────────────────────────────────────────────────────
    bool baro_read_ok = false;
    float pressure_hpa = 0.0f, temp_c = 0.0f;
    if (s_bmp_ok && hal_i2c_lock(100)) {
        if (s_bmp.performReading()) {
            pressure_hpa = s_bmp.pressure;
            temp_c       = s_bmp.temperature;
            baro_read_ok = true;
        }
        hal_i2c_unlock();
    }

    if (baro_read_ok) {
        baro_fusion_update(now_ms, pressure_hpa, temp_c, &snap->gps, &snap->baro);
    } else {
        snap->baro.valid = false;
    }

    snap->sys.uptime_s = (now_ms - boot_ms) / 1000;
}

void hal_sensors_retry_i2c(void) {
    if (!s_bmp_ok) s_bmp_ok = init_bmp580();
#if defined(BOARD_HAS_IMU_QMI8658) && BOARD_HAS_IMU_QMI8658
    if (!s_qmi_ok) s_qmi_ok = qmi8658_init();
#endif
}

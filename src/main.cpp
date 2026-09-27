/**
 * @file    main.cpp
 * @brief   ESP32-C6 Outdoor Tracker — Hardware Bring-Up Test  (v2 – crash fix)
 *
 * Milestone 5 — GPS, BMP580, AMOLED LVGL Dashboard, QMI8658 IMU
 */

#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BMP5xx.h>
#include <SensorQMI8658.hpp>
#include <lvgl.h>
#include "lcd_bsp.h"

// ── Biến giao diện (LVGL) ─────────────────────────────────────────────────
lv_obj_t * label_header;
lv_obj_t * label_compass;
lv_obj_t * label_speed;
lv_obj_t * label_coords;
lv_obj_t * label_imu;
lv_obj_t * label_env;

// ── Pin config (overridable từ platformio.ini build_flags) ──────────────────
#ifndef GPS_RX_PIN
#  define GPS_RX_PIN   2   // GPIO2 ← GPS TX
#endif
#ifndef GPS_TX_PIN
#  define GPS_TX_PIN   3   // GPIO3 → GPS RX
#endif
#ifndef GPS_BAUD
#  define GPS_BAUD  9600
#endif

#ifndef I2C_SDA_PIN
#  define I2C_SDA_PIN 18
#endif
#ifndef I2C_SCL_PIN
#  define I2C_SCL_PIN  8
#endif
#ifndef BMP580_I2C_ADDR
#  define BMP580_I2C_ADDR 0x47   // SDO=VCC → 0x47
#endif

// ── Timing ────────────────────────────────────────────────────────────────
static constexpr uint32_t PRINT_INTERVAL_MS = 2000;
static constexpr uint32_t GPS_TIMEOUT_MS    = 12000;

// ── Objects ───────────────────────────────────────────────────────────────
HardwareSerial  gpsSerial(1);   // UART1
TinyGPSPlus     gps;
Adafruit_BMP5xx bmp;
SensorQMI8658   qmi;

// ── State ─────────────────────────────────────────────────────────────────
static uint32_t tLastPrint = 0;
static uint32_t tLastGps   = 0;
static bool     bmpOk      = false;
static bool     imuOk      = false;
static float    imuPitch   = 0.0f;
static float    imuRoll    = 0.0f;

// ═══════════════════════════════════════════════════════════════════════════
//  I2C Bus Scanner
// ═══════════════════════════════════════════════════════════════════════════
static void i2cScan() {
    Serial.println(F("\n[I2C] ── Bus Scan ─────────────────────────────────"));
    Serial.println(F("        Address  │ Device"));
    Serial.println(F("        ─────────┼──────────────────────────────"));
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            const char* label = "Unknown";
            switch (addr) {
                case 0x15: label = "CST816  Touch controller"; break;
                case 0x46: label = "BMP580  (SDO = GND)     "; break;
                case 0x47: label = "BMP580  (SDO = VCC)     "; break;
                case 0x6A: label = "QMI8658 IMU (built-in)  "; break;
                case 0x6B: label = "QMI8658 IMU (built-in)  "; break;
                case 0x7E: label = "Unknown (PMIC or Touch?)"; break;
                default:   label = "Unknown device          "; break;
            }
            Serial.printf("          0x%02X   │ %s\n", addr, label);
            found++;
        }
    }
    Serial.println(F("        ─────────────────────────────────────────────"));
    if (found == 0)
        Serial.println(F("  *** Khong tim thay thiet bi I2C! Kiem tra SDA/SCL. ***"));
    else
        Serial.printf("  Tong cong: %u thiet bi\n", found);
    Serial.println();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Init Sensors
// ═══════════════════════════════════════════════════════════════════════════
static bool initBMP() {
    Serial.printf("[BMP] Ket noi 0x%02X ... ", BMP580_I2C_ADDR);
    if (!bmp.begin(BMP580_I2C_ADDR, &Wire)) {
        Serial.println(F("THAT BAI. Kiem tra day I2C & chan SDO."));
        return false;
    }
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
    Serial.println(F("OK"));
    return true;
}

static bool initIMU() {
    Serial.printf("[IMU] Ket noi QMI8658 (0x6A) ... ");
    if (!qmi.begin(Wire, 0x6A, I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println(F("THAT BAI."));
        return false;
    }
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS, SensorQMI8658::GYR_ODR_896_8Hz, SensorQMI8658::LPF_MODE_3);
    qmi.enableAccelerometer();
    qmi.enableGyroscope();
    Serial.println(F("OK"));
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Print and UI Update
// ═══════════════════════════════════════════════════════════════════════════
static void update_lvgl_ui(float temp, float pres, float speed_kmh, uint32_t sats, double lat, double lng, float alt, float course, bool gps_valid, bool bmp_valid, float pitch, float roll) {
    if (example_lvgl_lock(-1)) {
        char buf[64];
        if (label_header) {
            snprintf(buf, sizeof(buf), "Sats: %lu | Bat: 100%%", sats);
            lv_label_set_text(label_header, buf);
        }
        
        if (label_compass) {
            const char* dir = "N";
            if (course < 22.5 || course >= 337.5) dir = "N";
            else if (course < 67.5) dir = "NE";
            else if (course < 112.5) dir = "E";
            else if (course < 157.5) dir = "SE";
            else if (course < 202.5) dir = "S";
            else if (course < 247.5) dir = "SW";
            else if (course < 292.5) dir = "W";
            else if (course < 337.5) dir = "NW";
            // Removed gps_valid check to always show test data
            snprintf(buf, sizeof(buf), "%s (%.0f deg)", dir, course);
            lv_label_set_text(label_compass, buf);
        }

        if (label_speed) {
            snprintf(buf, sizeof(buf), "%.1f km/h", speed_kmh);
            lv_label_set_text(label_speed, buf);
        }
        
        if (label_coords) {
            if (gps_valid) {
                snprintf(buf, sizeof(buf), "Lat: %.6f\nLng: %.6f\nAlt: %.1f m", lat, lng, alt);
            } else {
                snprintf(buf, sizeof(buf), "Lat: --.------\nLng: --.------\nAlt: -- m");
            }
            lv_label_set_text(label_coords, buf);
        }
        
        if (label_env) {
            if (bmp_valid) {
                snprintf(buf, sizeof(buf), "T: %.1f C | P: %.1f hPa", temp, pres);
            } else {
                snprintf(buf, sizeof(buf), "T: -- C | P: -- hPa");
            }
            lv_label_set_text(label_env, buf);
        }
        
        if (label_imu) {
            snprintf(buf, sizeof(buf), "Tilt: P:%.0f  R:%.0f", pitch, roll);
            lv_label_set_text(label_imu, buf);
        }
        example_lvgl_unlock();
    }
}

static void printData() {
    Serial.println(F("\n=================================================="));
    Serial.println(F("  [GPS] ATGM336H"));
    
    double lat = 0.0, lng = 0.0;
    float speed = 0.0, alt = 0.0, course = 0.0;
    uint32_t sats = 0;
    bool gps_valid = false;
    
    if (gps.location.isValid()) {
        lat = gps.location.lat();
        lng = gps.location.lng();
        gps_valid = true;
        Serial.printf("    Lat / Lon  : %+.6f,  %+.6f\n", lat, lng);
    } else {
        Serial.println(F("    Vi tri     : Chua fix (dang tim ve tinh...)"));
    }
    
    if (gps.speed.isValid()) speed = gps.speed.kmph();
    if (gps.satellites.isValid()) sats = gps.satellites.value();
    if (gps.altitude.isValid()) alt = gps.altitude.meters();
    if (gps.course.isValid()) course = gps.course.deg();
    
    Serial.printf("    Toc do     : %.1f km/h\n", speed);
    Serial.printf("    Ve tinh    : %u\n", sats);
    Serial.printf("    Altitude   : %.1f m (GPS)\n", alt);
    Serial.printf("    Course     : %.1f°\n", course);
    Serial.printf("    NMEA       : chars=%lu  fixes=%lu  err=%lu\n", gps.charsProcessed(), gps.sentencesWithFix(), gps.failedChecksum());

    Serial.println(F("  [BMP] BMP580"));
    float temp = 0.0, pres = 0.0;
    bool bmp_valid = false;
    if (!bmpOk) {
        Serial.println(F("    Sensor chua san sang - kiem tra I2C."));
    } else if (!bmp.performReading()) {
        Serial.println(F("    ERROR: performReading() that bai!"));
    } else {
        pres = bmp.pressure;
        temp = bmp.temperature;
        bmp_valid = true;
        Serial.printf("    Nhiet do   : %.2f C\n", temp);
        Serial.printf("    Ap suat    : %.2f hPa\n", pres);
    }
    
    Serial.println(F("  [IMU] QMI8658"));
    if (imuOk) {
        float ax, ay, az;
        if (qmi.getAccelerometer(ax, ay, az)) {
            imuPitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
            imuRoll  = atan2(ay, az) * 180.0 / PI;
            Serial.printf("    Pitch/Roll : %.1f° / %.1f°\n", imuPitch, imuRoll);
        }
    } else {
        Serial.println(F("    IMU offline."));
    }
    
    Serial.println(F("==================================================\n"));
    
    update_lvgl_ui(temp, pres, speed, sats, lat, lng, alt, course, gps_valid, bmp_valid, imuPitch, imuRoll);
}

// ═══════════════════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════════════════
static void build_dashboard() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    label_header = lv_label_create(lv_scr_act());
    lv_label_set_text(label_header, "Sats: 0 | Bat: 100%");
    lv_obj_set_style_text_color(label_header, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(label_header, &lv_font_montserrat_20, 0);
    lv_obj_align(label_header, LV_ALIGN_TOP_MID, 0, 10);

    label_compass = lv_label_create(lv_scr_act());
    lv_label_set_text(label_compass, "N (0 deg)");
    lv_obj_set_style_text_color(label_compass, lv_color_hex(0xFF8800), 0);
    lv_obj_set_style_text_font(label_compass, &lv_font_montserrat_32, 0);
    lv_obj_align(label_compass, LV_ALIGN_TOP_MID, 0, 50);

    label_speed = lv_label_create(lv_scr_act());
    lv_label_set_text(label_speed, "0.0 km/h");
    lv_obj_set_style_text_color(label_speed, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_speed, &lv_font_montserrat_32, 0);
    lv_obj_align(label_speed, LV_ALIGN_CENTER, 0, -40);

    label_coords = lv_label_create(lv_scr_act());
    lv_label_set_text(label_coords, "Lat: --.------\nLng: --.------\nAlt: -- m");
    lv_obj_set_style_text_color(label_coords, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(label_coords, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(label_coords, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_coords, LV_ALIGN_CENTER, 0, 40);

    label_imu = lv_label_create(lv_scr_act());
    lv_label_set_text(label_imu, "Tilt: P: 0  R: 0");
    lv_obj_set_style_text_color(label_imu, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(label_imu, &lv_font_montserrat_20, 0);
    lv_obj_align(label_imu, LV_ALIGN_BOTTOM_MID, 0, -45);

    label_env = lv_label_create(lv_scr_act());
    lv_label_set_text(label_env, "T: -- C | P: -- hPa");
    lv_obj_set_style_text_color(label_env, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(label_env, &lv_font_montserrat_20, 0);
    lv_obj_align(label_env, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void setup() {
    Serial.begin(115200);
    delay(2000);   // chờ USB CDC enumerate
    
    Serial.println(F("\n\n╔══════════════════════════════════════════════════╗"));
    Serial.println(F("║  ESP32-C6 Outdoor Tracker – Bring-Up v4       ║"));
    Serial.println(F("╚══════════════════════════════════════════════════╝\n"));
    Serial.flush();

    Serial.printf("[I2C] Init: SDA=GPIO%d  SCL=GPIO%d  400kHz\n", I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000UL);
    delay(150);
    i2cScan();
    Serial.flush();

    bmpOk = initBMP();
    imuOk = initIMU();
    Serial.flush();

    Serial.printf("[GPS] Init UART1: RX=GPIO%d  TX=GPIO%d  %d baud\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    Serial.flush();
    delay(100);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    tLastGps = millis();
    Serial.println(F("[GPS] Dang cho NMEA data...\n"));

    Serial.println(F("[SYS] Khoi tao man hinh AMOLED..."));
    lcd_lvgl_Init();

    if (example_lvgl_lock(-1)) {
        build_dashboard();
        example_lvgl_unlock();
    }
    Serial.println(F("[SYS] Man hinh OK. Vao vong lap chinh...\n"));
    Serial.flush();
}

void loop() {
    const uint32_t now = millis();

    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        if (gps.encode(c)) {
            tLastGps = now;
        }
    }

    if (now - tLastGps > GPS_TIMEOUT_MS) {
        Serial.println(F("[GPS] ⚠️ Khong nhan du lieu >12s!"));
        tLastGps = now;
    }

    if (now - tLastPrint >= PRINT_INTERVAL_MS) {
        tLastPrint = now;
        
        static uint8_t printCount = 0;
        if (printCount++ % 5 == 0) {
            i2cScan();
            if (!bmpOk) bmpOk = initBMP();
            if (!imuOk) imuOk = initIMU();
        }
        
        printData();
    }
}

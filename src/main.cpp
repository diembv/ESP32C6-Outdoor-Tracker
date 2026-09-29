/**
 * @file    main.cpp
 * @brief   ESP32-C6 Outdoor Tracker — Milestone 4: LVGL 3-page Dashboard
 *
 * Board  : Waveshare ESP32-C6-Touch-AMOLED-1.64
 * Screen : SH8601 AMOLED 280 × 456 px (QSPI)
 * MCU    : ESP32-C6 RISC-V @ 160 MHz, 512 KB SRAM, no PSRAM
 *
 * QMI8658 NOTE:
 *   SensorLib @ 0.5.0 dùng ESP-IDF driver_ng (i2c_master new API) nội bộ.
 *   Wire.begin() dùng driver cũ (i2c_driver_install). Hai driver không thể
 *   cùng tồn tại trên một I2C peripheral → abort() crash loop.
 *   Fix: đọc QMI8658 trực tiếp qua Wire (không dùng SensorLib).
 */

#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BMP5xx.h>
#include <math.h>
#include <lvgl.h>
#include "lcd_bsp.h"
#include "ui_dashboard.h"
#include "sd_logger.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  Pin Config
// ═══════════════════════════════════════════════════════════════════════════════
#ifndef GPS_RX_PIN
#  define GPS_RX_PIN   2
#endif
#ifndef GPS_TX_PIN
#  define GPS_TX_PIN   3
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
#  define BMP580_I2C_ADDR 0x47
#endif

// ═══════════════════════════════════════════════════════════════════════════════
//  QMI8658 — Direct Wire Driver (không dùng SensorLib)
//
//  SensorLib 0.5.0 gây I2C driver_ng conflict với Wire → crash loop.
//  Giải pháp: đọc thẳng thanh ghi QMI8658 qua Wire.
//
//  Datasheet: QMI8658A, I2C addr 0x6A (SA0=GND) / 0x6B (SA0=VCC)
// ═══════════════════════════════════════════════════════════════════════════════
#define QMI8658_ADDR        0x6A    // board Waveshare: SA0=GND → 0x6A
#define QMI_REG_WHO_AM_I    0x00    // returns 0x05
#define QMI_REG_CTRL1       0x02    // SPI/I2C mode
#define QMI_REG_CTRL2       0x03    // Accel config: range + ODR
#define QMI_REG_CTRL7       0x08    // Enable sensors
#define QMI_REG_AX_L        0x35    // Accel X low byte (6 regs: AX,AY,AZ)
#define QMI_REG_STATUS0     0x2E    // Data ready status

static bool     qmiOk    = false;
static float    imuPitch = 0.0f;
static float    imuRoll  = 0.0f;

static uint8_t qmiAddress = QMI8658_ADDR;

// ── Đọc n bytes từ thanh ghi QMI8658 ─────────────────────────────────────────
static bool qmiReadRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(qmiAddress);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0) return false;
    uint8_t got = Wire.requestFrom((uint8_t)qmiAddress, len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

// ── Ghi 1 byte vào thanh ghi QMI8658 ────────────────────────────────────────
static bool qmiWriteReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(qmiAddress);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

// ── Khởi tạo QMI8658 ─────────────────────────────────────────────────────────
static bool initQMI8658() {
    Serial.print(F("[IMU] QMI8658 WHO_AM_I ... "));
    uint8_t who = 0;
    qmiAddress = 0x6A; // Thử địa chỉ mặc định trước
    if (!qmiReadRegs(QMI_REG_WHO_AM_I, &who, 1) || who != 0x05) {
        qmiAddress = 0x6B; // Fallback sang 0x6B
        if (!qmiReadRegs(QMI_REG_WHO_AM_I, &who, 1) || who != 0x05) {
            Serial.println(F("I2C error (Thử cả 0x6A và 0x6B)."));
            return false;
        }
    }
    Serial.printf("OK (0x%02X tại 0x%02X)\n", who, qmiAddress);

    if (!qmiWriteReg(QMI_REG_CTRL1, 0x40)) return false;
    if (!qmiWriteReg(QMI_REG_CTRL2, 0x13)) return false;
    if (!qmiWriteReg(QMI_REG_CTRL7, 0x01)) return false;
    delay(10);
    return true;
}

// ── Đọc accelerometer → tính Pitch / Roll ────────────────────────────────────
static void readQMI8658(float &pitch, float &roll) {
    uint8_t raw[6];
    if (!qmiReadRegs(QMI_REG_AX_L, raw, 6)) return;

    int16_t ax_raw = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ay_raw = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t az_raw = (int16_t)((raw[5] << 8) | raw[4]);

    // ±4g range → sensitivity = 8192 LSB/g
    float ax = ax_raw / 8192.0f;
    float ay = ay_raw / 8192.0f;
    float az = az_raw / 8192.0f;

    pitch = atan2f(-ax, sqrtf(ay*ay + az*az)) * 180.0f / (float)M_PI;
    roll  = atan2f( ay, az)                   * 180.0f / (float)M_PI;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Timing
// ═══════════════════════════════════════════════════════════════════════════════
static constexpr uint32_t SENSOR_READ_MS  = 200;  // 5Hz
static constexpr uint32_t UI_UPDATE_MS    = 250;  // 4Hz
static constexpr uint32_t SERIAL_PRINT_MS = 2000;
static constexpr uint32_t GPS_TIMEOUT_MS  = 12000;
static constexpr uint32_t I2C_RETRY_EVERY = 10;

// ═══════════════════════════════════════════════════════════════════════════════
//  Hardware Objects
// ═══════════════════════════════════════════════════════════════════════════════
HardwareSerial  gpsSerial(1);
TinyGPSPlus     gpsParser;
Adafruit_BMP5xx bmp;

// ═══════════════════════════════════════════════════════════════════════════════
//  State
// ═══════════════════════════════════════════════════════════════════════════════
static bool     bmpOk        = false;
static uint32_t tLastSensor  = 0;
static uint32_t tLastUiUpd   = 0;
static uint32_t tLastPrint   = 0;
static uint32_t tLastGps     = 0;
static uint32_t tBoot        = 0;
static uint8_t  printCount   = 0;

SensorSnapshot g_snap = {};

// ═══════════════════════════════════════════════════════════════════════════════
//  I2C Scanner
// ═══════════════════════════════════════════════════════════════════════════════
static void i2cScan() {
    Serial.println(F("\n[I2C] ── Bus Scan ──────────────────────────────────"));
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            const char *label = "Unknown";
            switch (addr) {
                case 0x38: label = "FT3168  Touch (0x38)    "; break;
                case 0x46: label = "BMP580  (SDO=GND, 0x46) "; break;
                case 0x47: label = "BMP580  (SDO=VCC, 0x47) "; break;
                case 0x6A: label = "QMI8658 IMU (SA0=GND)   "; break;
                case 0x6B: label = "QMI8658 IMU (SA0=VCC)   "; break;
                case 0x7E: label = "Unknown (PMIC/Touch?)   "; break;
                default:   label = "Unknown device          "; break;
            }
            Serial.printf("  0x%02X → %s\n", addr, label);
            found++;
        }
    }
    if (found == 0)
        Serial.println(F("  *** Không tìm thấy thiết bị I2C! ***"));
    else
        Serial.printf("  Tổng: %u thiết bị\n", found);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  BMP580 Init
// ═══════════════════════════════════════════════════════════════════════════════
static bool initBMP() {
    Serial.printf("[BMP] Kết nối 0x%02X ... ", BMP580_I2C_ADDR);
    if (!bmp.begin(BMP580_I2C_ADDR, &Wire)) {
        Serial.println(F("THẤT BẠI."));
        return false;
    }
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
    Serial.println(F("OK"));
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Đọc sensors → g_snap
// ═══════════════════════════════════════════════════════════════════════════════
static void readSensors() {
    uint32_t now = millis();
    static uint32_t lastSlowRead = 0;
    bool doSlowRead = (now - lastSlowRead >= 1000) || (lastSlowRead == 0);

    if (doSlowRead) {
        lastSlowRead = now;

        // ── GPS ───────────────────────────────────────────────────────────────────
        g_snap.gps.fix_valid    = gpsParser.location.isValid();
        g_snap.gps.satellites   = gpsParser.satellites.isValid() ?
                                   gpsParser.satellites.value() : 0;
        g_snap.gps.speed_kmh    = gpsParser.speed.isValid() ?
                                   gpsParser.speed.kmph() : 0.0f;
        g_snap.gps.altitude_m   = gpsParser.altitude.isValid() ?
                                   gpsParser.altitude.meters() : 0.0f;
        g_snap.gps.course_deg   = gpsParser.course.isValid() ?
                                   (float)gpsParser.course.deg() : 0.0f;
        g_snap.gps.chars_proc   = gpsParser.charsProcessed();
        g_snap.gps.fixes        = gpsParser.sentencesWithFix();
        g_snap.gps.checksum_err = gpsParser.failedChecksum();
        if (g_snap.gps.fix_valid) {
            g_snap.gps.latitude  = gpsParser.location.lat();
            g_snap.gps.longitude = gpsParser.location.lng();
        }

        // ── GPS Time & Date ───────────────────────────────────────────────────────
        g_snap.gps.date_valid = gpsParser.date.isValid();
        if (g_snap.gps.date_valid) {
            g_snap.gps.year  = gpsParser.date.year();
            g_snap.gps.month = gpsParser.date.month();
            g_snap.gps.day   = gpsParser.date.day();
        }

        g_snap.gps.time_valid = gpsParser.time.isValid();
        if (g_snap.gps.time_valid) {
            // Store raw UTC time. We will convert to local time in the UI
            g_snap.gps.hour   = gpsParser.time.hour();
            g_snap.gps.minute = gpsParser.time.minute();
            g_snap.gps.second = gpsParser.time.second();
        }

        // ── BMP580 ────────────────────────────────────────────────────────────────
        if (bmpOk && bmp.performReading()) {
            g_snap.baro.pressure_hpa  = bmp.pressure;
            g_snap.baro.temperature_c = bmp.temperature;
            float ratio = g_snap.baro.pressure_hpa / 1013.25f;
            g_snap.baro.altitude_m = 44330.0f * (1.0f - powf(ratio, 0.1903f));
            g_snap.baro.valid = true;
        } else {
            g_snap.baro.valid = false;
        }
        
        g_snap.sys.uptime_s = (now - tBoot) / 1000;
    }

    // ── QMI8658 (Direct Wire - Đọc mỗi 200ms / 5Hz) ───────────────────────────
    if (qmiOk) {
        readQMI8658(imuPitch, imuRoll);
        g_snap.imu.pitch_deg = imuPitch;
        g_snap.imu.roll_deg  = imuRoll;
        g_snap.imu.valid = true;
    } else {
        g_snap.imu.valid = false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Serial print (dùng integer split, không dùng %f)
// ═══════════════════════════════════════════════════════════════════════════════
static void printSerial() {
    Serial.println(F("\n=================================================="));
    Serial.println(F("  [GPS] ATGM336H"));
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
        Serial.printf("    Speed  : %ld.%01ld km/h  Sats: %lu  Course: %ld deg\n",
            sI, sD, (unsigned long)g_snap.gps.satellites,
            (long)g_snap.gps.course_deg);
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
        Serial.printf("    T: %ld.%02ld C  P: %ld.%02ld hPa  Alt: %ld.%01ld m\n",
            tI, labs(tD), pI, labs(pD), aI, labs(aD));
    } else {
        Serial.println(F("    Sensor chưa sẵn sàng."));
    }

    Serial.println(F("  [IMU] QMI8658 (direct Wire)"));
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
    delay(2000);
    tBoot = millis();

    Serial.println(F("\n╔══════════════════════════════════════════════════╗"));
    Serial.println(F("║  ESP32-C6 Outdoor Tracker — Milestone 4         ║"));
    Serial.println(F("║  LVGL Dashboard + QMI8658 Direct Wire           ║"));
    Serial.println(F("╚══════════════════════════════════════════════════╝\n"));
    Serial.flush();

    // Vô hiệu hóa thẻ SD ngay từ đầu bằng cách kéo chân CS lên HIGH
    // Đảm bảo thẻ SD không làm nhiễu bus SPI (đặc biệt là tín hiệu MISO) khi AMOLED khởi tạo
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    
    // ── I2C ──────────────────────────────────────────────────────────────────
    Serial.printf("[I2C] Init: SDA=GPIO%d  SCL=GPIO%d  100kHz\n",
                  I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000UL); // Hạ xuống 100kHz để tăng độ ổn định cho cảm biến và touch
    Wire.setTimeout(20);
    delay(150);
    i2cScan();
    Serial.flush();

    // ── BMP580 ────────────────────────────────────────────────────────────────
    bmpOk = initBMP();

    // ── QMI8658 Direct Wire ───────────────────────────────────────────────────
    qmiOk = initQMI8658();
    Serial.flush();

    // ── GPS ───────────────────────────────────────────────────────────────────
    Serial.printf("[GPS] Init UART1: RX=GPIO%d  TX=GPIO%d  %d baud\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    tLastGps = millis();
    Serial.println(F("[GPS] Đang chờ NMEA data...\n"));
    Serial.flush();

    // ── Display + LVGL (BSP: FreeRTOS LVGL task) ─────────────────────────────
    Serial.println(F("[SYS] Khởi tạo AMOLED + LVGL..."));
    lcd_lvgl_Init();

    // ── Build Dashboard UI ────────────────────────────────────────────────────
    if (example_lvgl_lock(-1)) {
        ui_dashboard_init();
        example_lvgl_unlock();
    }
    Serial.println(F("[SYS] Dashboard OK — swipe để chuyển trang.\n"));
    Serial.flush();

    // ── MicroSD Logger (SPI2, CS=GPIO15) ──────────────────────────────────────
    if (example_lvgl_lock(-1)) {
        sd_logger_init(&g_snap);
        example_lvgl_unlock();
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

    // ── Đọc ADC Pin (Mỗi 2 giây) ─────────────────────────────────────────────
    static uint32_t lastBat = 0;
    if (now - lastBat >= 2000) {
        lastBat = now;
        analogReadResolution(12); // Đảm bảo 12-bit
        int adc = analogRead(0);
        float volts = (adc / 4095.0f) * 3.3f * 2.0f;
        g_snap.sys.battery_v = volts;
        
        // Map Volts -> % (Xấp xỉ cho Li-Po 3.7V)
        if (volts >= 4.15f) g_snap.sys.battery_pct = 100;
        else if (volts <= 3.30f) g_snap.sys.battery_pct = 0;
        else g_snap.sys.battery_pct = (uint8_t)(((volts - 3.30f) / (4.15f - 3.30f)) * 100.0f);
    }

    // ── 1. Feed GPS parser ────────────────────────────────────────────────────
    while (gpsSerial.available()) {
        if (gpsParser.encode((char)gpsSerial.read())) {
            tLastGps = now;
        }
    }

    // ── 2. GPS timeout warn ───────────────────────────────────────────────────
    if (now - tLastGps > GPS_TIMEOUT_MS) {
        Serial.println(F("[GPS] ⚠️ Không nhận dữ liệu > 12s!"));
        tLastGps = now;
    }

    // ── 3. Đọc sensor mỗi 500ms ──────────────────────────────────────────────
    if (now - tLastSensor >= SENSOR_READ_MS) {
        tLastSensor = now;
        if (example_lvgl_lock(-1)) {
            readSensors();
            example_lvgl_unlock();
        }
    }

    // ── 4. Update LVGL UI ────────────────────────────────────────────────────
    if (now - tLastUiUpd >= UI_UPDATE_MS) {
        tLastUiUpd = now;
        if (example_lvgl_lock(-1)) {
            ui_dashboard_update(&g_snap);
            example_lvgl_unlock();
        }
    }

    // ── 5. Serial debug mỗi 2s ───────────────────────────────────────────────
    if (now - tLastPrint >= SERIAL_PRINT_MS) {
        tLastPrint = now;
        if (printCount++ % I2C_RETRY_EVERY == 0) {
            if (example_lvgl_lock(-1)) {
                if (!bmpOk) bmpOk = initBMP();
                if (!qmiOk) qmiOk = initQMI8658();
                example_lvgl_unlock();
            }
        }
        printSerial();
    }

    // ── 6. Ghi log thẻ nhớ định kỳ (mỗi 1 giây) ──────────────────────────────
    static uint32_t tLastSdLog = 0;
    if (now - tLastSdLog >= 1000) {
        tLastSdLog = now;
        if (example_lvgl_lock(-1)) {
            sd_logger_log(&g_snap);
            example_lvgl_unlock();
        }
    }

    // ── 7. Yield cho LVGL FreeRTOS task ──────────────────────────────────────
    delay(5);
}

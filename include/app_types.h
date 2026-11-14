/**
 * @file app_types.h
 * @brief Shared hardware-independent domain data structures.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double   latitude;       // Decimal degrees (N+, S-)
    double   longitude;      // Decimal degrees (E+, W-)
    float    speed_kmh;      // Speed in km/h
    float    altitude_m;     // GPS altitude (m)
    float    course_deg;     // Course over ground 0-359.9 deg
    uint32_t satellites;     // Locked satellites count
    bool     fix_valid;      // true = valid GPS location fix

    // GPS Time & Date (UTC)
    bool     time_valid;
    bool     date_valid;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    uint32_t chars_proc;     // NMEA chars processed
    uint32_t fixes;          // Sentences with fix
    uint32_t checksum_err;   // Failed checksum count

    bool     altitude_valid; // GPS altitude validity
    float    hdop;           // Horizontal Dilution of Precision (99.9 = invalid)
} GpsData;

typedef struct {
    float pressure_hpa;      // Barometric pressure in hPa
    float temperature_c;     // Temperature in Celsius
    float altitude_m;        // Calculated barometric altitude (m)
    bool  valid;             // Sensor reading valid
    bool  calibrated;        // true = sea-level reference P0 locked by GPS
    float sea_level_hpa;     // Current P0 reference (default 1013.25)
} BaroData;

typedef struct {
    float pitch_deg;         // Pitch angle (deg) from accelerometer
    float roll_deg;          // Roll angle (deg) from accelerometer
    bool  valid;             // Sensor reading valid
} ImuData;

typedef enum {
    ACTIVITY_MODE_HIKE = 0,
    ACTIVITY_MODE_BIKE = 1
} ActivityMode;

typedef struct {
    uint32_t     uptime_s;          // System uptime in seconds
    float        battery_v;         // Battery voltage (V)
    uint8_t      battery_pct;       // Battery percentage (0-100)
    bool         sd_present;        // SD card detected
    bool         sd_ok;             // SD card mounted OK
    uint32_t     sd_free_mb;        // SD capacity / free MB
    bool         is_logging_active; // Active CSV logging session
    ActivityMode activity_mode;     // HIKE or BIKE
    uint32_t     boot_count;        // Boot counter persisted across soft resets
    const char  *reset_reason;      // Last reset reason string
} SysData;

typedef struct {
    GpsData  gps;
    BaroData baro;
    ImuData  imu;
    SysData  sys;
} SensorSnapshot;

#ifdef __cplusplus
}
#endif

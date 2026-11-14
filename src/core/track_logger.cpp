/**
 * @file track_logger.cpp
 * @brief Hardware-independent Smart CSV Track Logger.
 */

#include "core/track_logger.h"
#include "hal/hal_storage.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#ifdef ARDUINO
#  include <Arduino.h>
#  define LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#  define LOG_PRINTLN(s)  Serial.println(s)
#else
#  define LOG_PRINTF(...) printf(__VA_ARGS__)
#  define LOG_PRINTLN(s)  printf("%s\n", s)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SD_LOG_DIR       "/sdcard/logs"
#define LOG_BUFFER_SIZE  1024

static FILE *s_log_file = NULL;
static char  s_filepath[128];

// Static RAM buffer for Compact CSV logging
static char     s_log_buffer[LOG_BUFFER_SIZE];
static size_t   s_log_buffer_len  = 0;
static uint32_t s_last_flush_time = 0;

// State Machine Tracking (Smart Logging)
static float        s_last_heading     = -1.0f;
static float        s_last_alt         = 0.0f;
static uint32_t     s_last_sample_time = 0;
static uint32_t     s_low_speed_start  = 0;
static float        s_session_distance = 0.0f;
static double       s_last_lat         = 0.0;
static double       s_last_lon         = 0.0;
static uint32_t     s_session_start_ms = 0;
static uint32_t     s_points_logged    = 0;
static ActivityMode s_activity_mode    = ACTIVITY_MODE_HIKE;

static float haversine_distance_m(double lat1, double lon1, double lat2, double lon2) {
    static constexpr double EARTH_RADIUS_M = 6371000.0;
    double dlat = (lat2 - lat1) * (M_PI / 180.0);
    double dlon = (lon2 - lon1) * (M_PI / 180.0);
    double rlat1 = lat1 * (M_PI / 180.0);
    double rlat2 = lat2 * (M_PI / 180.0);

    double a = sin(dlat * 0.5) * sin(dlat * 0.5) +
               cos(rlat1) * cos(rlat2) * sin(dlon * 0.5) * sin(dlon * 0.5);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return (float)(EARTH_RADIUS_M * c);
}

void track_logger_set_activity_mode(ActivityMode mode) {
    s_activity_mode = mode;
}

ActivityMode track_logger_get_activity_mode(void) {
    return s_activity_mode;
}

static uint32_t to_epoch_seconds(uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t min, uint8_t sec) {
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
    static const uint16_t days_before_month[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    uint32_t y = year - 1970;
    uint32_t leap_days = (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
    uint32_t days = y * 365 + leap_days + days_before_month[month - 1] + (day - 1);
    bool is_leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    if (is_leap && month > 2) days++;
    return days * 86400UL + hour * 3600UL + min * 60UL + sec;
}

static void flush_buffer_to_sd(uint32_t now_ms) {
    if (s_log_file && s_log_buffer_len > 0) {
        size_t written = fwrite(s_log_buffer, 1, s_log_buffer_len, s_log_file);
        if (written != s_log_buffer_len) {
            LOG_PRINTF("[SD] Canh bao: chi ghi duoc %u/%u byte\n",
                       (unsigned)written, (unsigned)s_log_buffer_len);
        }
        fflush(s_log_file);
        fsync(fileno(s_log_file));
        s_log_buffer_len  = 0;
        s_last_flush_time = now_ms;
    }
}

bool track_logger_init(SensorSnapshot *snap) {
    if (snap) {
        snap->sys.sd_present        = false;
        snap->sys.sd_ok             = false;
        snap->sys.sd_free_mb        = 0;
        snap->sys.is_logging_active = false;
    }

    uint32_t capacity_mb = 0;
    if (!hal_storage_init(&capacity_mb)) {
        return false;
    }

    if (snap) {
        snap->sys.sd_present    = true;
        snap->sys.sd_ok         = true;
        snap->sys.sd_free_mb    = capacity_mb;
        snap->sys.activity_mode = s_activity_mode;
    }

    if (hal_storage_ensure_dir(SD_LOG_DIR)) {
        LOG_PRINTLN("[SD] Thu muc /logs san sang");
    }

    return true;
}

void track_logger_toggle(SensorSnapshot *snap, uint32_t now_ms) {
    if (!hal_storage_is_ready() || !snap) return;

    if (snap->sys.is_logging_active) {
        snap->sys.is_logging_active = false;
        if (s_log_file) {
            flush_buffer_to_sd(now_ms);
            fclose(s_log_file);
            s_log_file = NULL;
            LOG_PRINTLN("[SD] Da DUNG ghi log va luu file CSV an toan.");
        } else {
            LOG_PRINTLN("[SD] Da DUNG che do cho ghi log (Chua co file nao duoc tao).");
        }
    } else {
        snap->sys.is_logging_active = true;
        s_log_buffer_len   = 0;
        s_points_logged    = 0;
        s_last_flush_time  = now_ms;
        s_last_heading     = -1.0f;
        s_session_distance = 0.0f;
        s_last_lat         = 0.0;
        s_last_lon         = 0.0;
        s_session_start_ms = now_ms;

        if (snap->gps.date_valid && snap->gps.time_valid) {
            const char *mode_prefix = (s_activity_mode == ACTIVITY_MODE_BIKE) ? "CYCLING" : "HIKING";
            snprintf(s_filepath, sizeof(s_filepath), SD_LOG_DIR "/%s_%04u%02u%02u_%02u%02u%02u.csv",
                     mode_prefix,
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);

            s_log_file = fopen(s_filepath, "w");
            if (s_log_file) {
                const char *header = "ts,lat,lon,alt\n";
                fwrite(header, 1, strlen(header), s_log_file);
                fflush(s_log_file);
                fsync(fileno(s_log_file));
                LOG_PRINTF("[SD] BAT DAU ghi log CSV (%s): %s\n", mode_prefix, s_filepath);
            } else {
                LOG_PRINTF("[SD] Loi tao file CSV moi: %s\n", s_filepath);
            }
        } else {
            s_log_file = NULL;
            const char *mode_prefix = (s_activity_mode == ACTIVITY_MODE_BIKE) ? "CYCLING" : "HIKING";
            LOG_PRINTF("[SD] Che do CHO: Cho GPS co Time Fix de tao file %s_YYYYMMDD_HHMMSS.csv...\n", mode_prefix);
        }
    }
}

void track_logger_log(SensorSnapshot *snap, uint32_t now_ms) {
    if (!hal_storage_is_ready() || !snap) return;
    if (!snap->sys.is_logging_active) return;

    static uint32_t last_bat_warn = 0;
    if (snap->sys.battery_v > 0.5f && snap->sys.battery_v < 3.30f) {
        if (now_ms - last_bat_warn >= 30000) {
            last_bat_warn = now_ms;
            LOG_PRINTF("[SD] Dien ap nguon: %.2fV (Nguon USB hoac pin yeu)\n", snap->sys.battery_v);
        }
    }

    if (s_log_file == NULL) {
        if (snap->gps.date_valid && snap->gps.time_valid) {
            const char *mode_prefix = (s_activity_mode == ACTIVITY_MODE_BIKE) ? "CYCLING" : "HIKING";
            snprintf(s_filepath, sizeof(s_filepath), SD_LOG_DIR "/%s_%04u%02u%02u_%02u%02u%02u.csv",
                     mode_prefix,
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);

            s_log_file = fopen(s_filepath, "w");
            if (s_log_file) {
                const char *header = "ts,lat,lon,alt\n";
                fwrite(header, 1, strlen(header), s_log_file);
                fflush(s_log_file);
                fsync(fileno(s_log_file));
                s_last_flush_time = now_ms;
                LOG_PRINTF("[SD] GPS da co Time Fix! TAO FILE (%s): %s\n", mode_prefix, s_filepath);
            } else {
                LOG_PRINTF("[SD] Loi tao file CSV moi: %s\n", s_filepath);
                return;
            }
        } else {
            return;
        }
    }

    if (!snap->gps.fix_valid) {
        if (s_log_buffer_len > 0 && now_ms - s_last_flush_time >= 60000) {
            flush_buffer_to_sd(now_ms);
        }
        return;
    }

    float v       = snap->gps.speed_kmh;
    float heading = snap->gps.course_deg;

    bool  has_ele = false;
    float alt     = 0.0f;
    if (snap->baro.valid) {
        alt     = snap->baro.altitude_m;
        has_ele = true;
    } else if (snap->gps.altitude_valid) {
        alt     = snap->gps.altitude_m;
        has_ele = true;
    }

    bool gps_quality_ok = (snap->gps.hdop <= 2.2f && snap->gps.satellites >= 5);

    double log_lat = s_last_lat;
    double log_lon = s_last_lon;

    if (gps_quality_ok) {
        if (s_last_lat != 0.0 && s_last_lon != 0.0) {
            float dist = haversine_distance_m(s_last_lat, s_last_lon,
                                              snap->gps.latitude, snap->gps.longitude);
            if (dist > 1.0f) {
                if (s_activity_mode == ACTIVITY_MODE_HIKE && has_ele && s_last_alt != 0.0f) {
                    float d_alt   = fabsf(alt - s_last_alt);
                    float dist_3d = sqrtf(dist * dist + d_alt * d_alt);
                    s_session_distance += dist_3d;
                } else {
                    s_session_distance += dist;
                }
                s_last_lat = snap->gps.latitude;
                s_last_lon = snap->gps.longitude;
            }
        } else {
            s_last_lat = snap->gps.latitude;
            s_last_lon = snap->gps.longitude;
        }
        log_lat = s_last_lat;
        log_lon = s_last_lon;
    } else {
        if (s_last_lat == 0.0 && s_last_lon == 0.0) {
            s_last_lat = snap->gps.latitude;
            s_last_lon = snap->gps.longitude;
        }
        log_lat = s_last_lat;
        log_lon = s_last_lon;
    }

    float speed_threshold = (s_activity_mode == ACTIVITY_MODE_HIKE) ? 0.6f : 1.5f;
    if (v < speed_threshold || !gps_quality_ok) {
        if (s_low_speed_start == 0) s_low_speed_start = now_ms;
    } else {
        s_low_speed_start = 0;
    }

    bool is_standby = (s_low_speed_start != 0 && (now_ms - s_low_speed_start) > 10000);

    float delta_heading = 0.0f;
    if (s_last_heading >= 0.0f) {
        delta_heading = fabsf(heading - s_last_heading);
        if (delta_heading > 180.0f) delta_heading = 360.0f - delta_heading;
    }
    float delta_alt = has_ele ? fabsf(alt - s_last_alt) : 0.0f;

    bool high_maneuver = false;
    if (s_activity_mode == ACTIVITY_MODE_HIKE) {
        high_maneuver = (delta_heading >= 25.0f || delta_alt >= 2.0f);
    } else {
        high_maneuver = (delta_heading >= 15.0f || v >= 25.0f || delta_alt >= 3.0f);
    }

    uint32_t sample_interval = 3000;
    if (high_maneuver) {
        sample_interval = 1000;
    } else if (is_standby) {
        sample_interval = 10000;
    }

    if (s_last_heading >= 0.0f && (now_ms - s_last_sample_time < sample_interval)) {
        if (now_ms - s_last_flush_time >= 60000) flush_buffer_to_sd(now_ms);
        return;
    }

    s_last_heading     = heading;
    if (has_ele) s_last_alt = alt;
    s_last_sample_time = now_ms;
    s_points_logged++;

    uint32_t epoch_time = 0;
    if (snap->gps.date_valid && snap->gps.time_valid) {
        epoch_time = to_epoch_seconds(snap->gps.year, snap->gps.month, snap->gps.day,
                                      snap->gps.hour, snap->gps.minute, snap->gps.second);
    }
    if (epoch_time == 0) {
        time_t t_now = time(NULL);
        if (t_now > 1672531199) {
            epoch_time = (uint32_t)t_now;
        }
    }

    char line_buf[64];
    int len = snprintf(line_buf, sizeof(line_buf), "%lu,%.6f,%.6f,%.1f\n",
                       (unsigned long)epoch_time,
                       log_lat, log_lon,
                       alt);

    if (len > 0 && len < (int)sizeof(line_buf)) {
        if (s_log_buffer_len + (size_t)len >= LOG_BUFFER_SIZE) {
            flush_buffer_to_sd(now_ms);
        }
        if (s_log_buffer_len + (size_t)len < LOG_BUFFER_SIZE) {
            memcpy(s_log_buffer + s_log_buffer_len, line_buf, (size_t)len);
            s_log_buffer_len += (size_t)len;
        }
    }

    if (s_log_buffer_len >= 512 || (now_ms - s_last_flush_time >= 60000)) {
        flush_buffer_to_sd(now_ms);
    }
}

bool track_logger_is_ok(void) {
    return hal_storage_is_ready();
}

void track_logger_flush(void) {
    if (s_log_file) {
        flush_buffer_to_sd(0);
    }
}

void track_logger_get_stats(uint32_t *points_logged, size_t *buffer_usage) {
    if (points_logged) *points_logged = s_points_logged;
    if (buffer_usage)  *buffer_usage  = s_log_buffer_len;
}

void track_logger_get_summary(float *distance_m, uint32_t *duration_s, uint32_t now_ms) {
    if (distance_m) *distance_m = s_session_distance;
    if (duration_s) *duration_s = s_session_start_ms > 0 ? (now_ms - s_session_start_ms) / 1000 : 0;
}

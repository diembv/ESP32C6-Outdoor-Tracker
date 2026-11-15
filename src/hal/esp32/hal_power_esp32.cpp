/**
 * @file hal_power_esp32.cpp
 * @brief ESP32 power management backend (AXP2101 PMIC / TCA9554 EXIO / ADC).
 */

#include "hal/hal_power.h"
#include "boards/board_select.h"
#include "hal/common/axp2101.h"
#include "hal/common/tca9554.h"

#include <Arduino.h>
#include "esp_sleep.h"

RTC_NOINIT_ATTR static uint32_t s_boot_magic;
RTC_NOINIT_ATTR static uint32_t s_boot_count;

static const char *reset_reason_str(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_EXT:       return "EXT";
#ifdef ESP_RST_USB
        case ESP_RST_USB:       return "USB_UPLOAD";
#endif
        default:                return "OTHER";
    }
}

void hal_power_track_boot(uint32_t *out_boot_count, const char **out_reset_reason) {
    esp_reset_reason_t rr = esp_reset_reason();
    if (rr == ESP_RST_POWERON || s_boot_magic != 0xB007CAFE) {
        s_boot_magic = 0xB007CAFE;
        s_boot_count = 0;
    }
    s_boot_count++;

    if (out_boot_count)   *out_boot_count   = s_boot_count;
    if (out_reset_reason) *out_reset_reason = reset_reason_str(rr);

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("[PWR] Woke up from Deep Sleep by BOOT button!");
    }
}

void hal_power_init(void) {
#if defined(BOARD_BTN_PWR_PIN) && (BOARD_BTN_PWR_PIN >= 0)
    pinMode(BOARD_BTN_PWR_PIN, INPUT_PULLUP);
#endif

#if defined(BOARD_HAS_PMIC_AXP2101) && BOARD_HAS_PMIC_AXP2101
    if (axp2101_init()) {
        Serial.println(F("[PWR] AXP2101 PMIC OK"));
    } else {
        Serial.println(F("[PWR] AXP2101 PMIC init FAILED"));
    }
#endif

#if defined(BOARD_HAS_IO_EXPANDER_TCA9554) && BOARD_HAS_IO_EXPANDER_TCA9554
    uint8_t exio_addr = 0x20;
#  ifdef BOARD_TCA9554_ADDR
    exio_addr = BOARD_TCA9554_ADDR;
#  endif
    if (tca9554_init(exio_addr)) {
        Serial.printf("[PWR] TCA9554 IO Expander OK (0x%02X)\n", exio_addr);
#  ifdef BOARD_EXIO_AMOLED_EN_PIN
        tca9554_set_dir(BOARD_EXIO_AMOLED_EN_PIN, true);
        tca9554_write_pin(BOARD_EXIO_AMOLED_EN_PIN, true);
        delay(20);
#  endif
    } else {
        Serial.printf("[PWR] TCA9554 IO Expander FAILED (0x%02X)\n", exio_addr);
    }
#endif
}

void hal_power_read_battery(float *out_volts, uint8_t *out_pct) {
    float   volts = 0.0f;
    uint8_t pct   = 0;

#if defined(BOARD_HAS_PMIC_AXP2101) && BOARD_HAS_PMIC_AXP2101
    if (axp2101_is_ready() && axp2101_read_battery(&volts, &pct, NULL)) {
        if (out_volts) *out_volts = volts;
        if (out_pct)   *out_pct   = pct;
        return;
    }
#endif

#if defined(BOARD_BAT_ADC_PIN) && (BOARD_BAT_ADC_PIN >= 0)
    uint32_t mv = analogReadMilliVolts(BOARD_BAT_ADC_PIN);
    volts = (mv * BOARD_BAT_ADC_DIVIDER) / 1000.0f;
    if (volts >= 4.15f)      pct = 100;
    else if (volts <= 3.30f) pct = 0;
    else                     pct = (uint8_t)(((volts - 3.30f) / (4.15f - 3.30f)) * 100.0f);
#endif

    if (out_volts) *out_volts = volts;
    if (out_pct)   *out_pct   = pct;
}

bool hal_power_is_button_pressed(void) {
#if defined(BOARD_BTN_PWR_PIN) && (BOARD_BTN_PWR_PIN >= 0)
    return digitalRead(BOARD_BTN_PWR_PIN) == LOW;
#else
    return false;
#endif
}

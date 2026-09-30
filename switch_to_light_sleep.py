import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target_deep_sleep = '''        if (!g_snap.sys.is_logging_active) {
            Serial.println("[PWR] Khong ghi log -> DEEP SLEEP (An nut BOOT de thuc day)!");
            Serial.flush();
            esp_deep_sleep_start();
        } else {
            Serial.println("[PWR] Dang ghi log -> Chuan bi LIGHT SLEEP");
            // Khong dung setCpuFrequencyMhz vi gay lech baudrate
        }'''

replacement_light_sleep = '''        if (!g_snap.sys.is_logging_active) {
            Serial.println("[PWR] Khong ghi log -> LIGHT SLEEP (Tiet kiem pin, an BOOT de thuc day)!");
        } else {
            Serial.println("[PWR] Dang ghi log -> LIGHT SLEEP (Duy tri doc GPS)!");
        }'''

text = text.replace(target_deep_sleep, replacement_light_sleep)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = """        setCpuFrequencyMhz(80); // Hạ xung nhịp CPU xuống 80MHz tiết kiệm pin
        set_amoled_backlight(0); // Tắt đèn AMOLED (hoặc sleep màn)"""

replacement = """        set_amoled_backlight(0);
        
        if (!g_snap.sys.is_logging_active) {
            Serial.println("[PWR] Khong ghi log -> DEEP SLEEP (An nut BOOT de thuc day)!");
            Serial.flush();
            esp_deep_sleep_start();
        } else {
            Serial.println("[PWR] Dang ghi log -> Ha xung 80MHz & san sang Light Sleep");
            setCpuFrequencyMhz(80);
        }"""

text = text.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

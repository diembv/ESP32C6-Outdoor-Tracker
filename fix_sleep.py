import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Fix the SD Card serial print
target_serial = '''      if (g_snap.sys.sd_ok) {
        Serial.printf("  [SYS] SD Card: OK (%lu MB) - logging\\n", (unsigned long)g_snap.sys.sd_free_mb);
      } else {'''

replacement_serial = '''      if (g_snap.sys.sd_ok) {
        if (g_snap.sys.is_logging_active) {
            Serial.printf("  [SYS] SD Card: OK (%lu MB) - LOGGING\\n", (unsigned long)g_snap.sys.sd_free_mb);
        } else {
            Serial.printf("  [SYS] SD Card: OK (%lu MB) - IDLE\\n", (unsigned long)g_snap.sys.sd_free_mb);
        }
      } else {'''

text = text.replace(target_serial, replacement_serial)

# 2. Fix the loop logic to use standard light sleep instead of setCpuFrequencyMhz(80)
target_loop = '''        set_amoled_backlight(0);
        
        if (!g_snap.sys.is_logging_active) {
            Serial.println("[PWR] Khong ghi log -> DEEP SLEEP (An nut BOOT de thuc day)!");
            Serial.flush();
            esp_deep_sleep_start();
        } else {
            Serial.println("[PWR] Dang ghi log -> Ha xung 80MHz & san sang Light Sleep");
            setCpuFrequencyMhz(80);
        }'''

replacement_loop = '''        set_amoled_backlight(0);
        
        if (!g_snap.sys.is_logging_active) {
            Serial.println("[PWR] Khong ghi log -> DEEP SLEEP (An nut BOOT de thuc day)!");
            Serial.flush();
            esp_deep_sleep_start();
        } else {
            Serial.println("[PWR] Dang ghi log -> Chuan bi LIGHT SLEEP");
            // Khong dung setCpuFrequencyMhz vi gay lech baudrate
        }'''

text = text.replace(target_loop, replacement_loop)


target_loop2 = '''        setCpuFrequencyMhz(160); // Khôi phục sức mạnh đồ họa LVGL
        set_amoled_backlight(255); // Bật lại AMOLED'''

replacement_loop2 = '''        // setCpuFrequencyMhz(160); da bi loai bo
        set_amoled_backlight(255); // Bat lai AMOLED'''

text = text.replace(target_loop2, replacement_loop2)

target_loop3 = '''        // Khi màn hình tắt, ta bỏ qua hoàn toàn việc dựng hình LVGL
        // Chỉ feed GPS để duy trì tracking ngầm
        while (gpsSerial.available()) gpsParser.encode((char)gpsSerial.read());
        
        static uint32_t tLastSdLog = 0;
        if (now - tLastSdLog >= 1000) {
            tLastSdLog = now;
            sd_logger_log(&g_snap);
        }
        delay(10);
        return; // Thoát sớm loop(), KHÔNG chạy logic UI bên dưới'''

replacement_loop3 = '''        // Khi man hinh tat va is_logging_active == true
        while (gpsSerial.available()) gpsParser.encode((char)gpsSerial.read());
        
        static uint32_t tLastSdLog = 0;
        if (now - tLastSdLog >= 1000) {
            tLastSdLog = now;
            sd_logger_log(&g_snap);
        }
        
        // LIGHT SLEEP CHUAN ESP-IDF:
        // Cho phep danh thuc bang UART (khi co the NMEA bay vao)
        uart_set_wakeup_threshold(UART_NUM_1, 3);
        esp_sleep_enable_uart_wakeup(UART_NUM_1);
        
        // Ngu nong khoang 100ms de tiet kiem pin, hoac bi thuc day boi UART som hon
        esp_sleep_enable_timer_wakeup(100000); 
        esp_light_sleep_start();
        
        return; // Thoat som, khong update UI'''

text = text.replace(target_loop3, replacement_loop3)


with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

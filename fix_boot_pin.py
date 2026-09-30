import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Remove from setup()
target_setup = '''    gpio_wakeup_enable((gpio_num_t)BTN_PWR_PIN, GPIO_INTR_LOW_LEVEL);
    
    // Kiem tra ly do thuc day
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("[PWR] Woke up from Deep Sleep by BOOT button!");
    }'''

replacement_setup = '''    // Da di doi gpio_wakeup_enable xuong luc di ngu'''

text = text.replace(target_setup, replacement_setup)

# 2. Fix the loop digitalRead
target_loop_btn = '''    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    // Kiem tra ky neu no luon LOW thi se chan timeout man hinh
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        static uint32_t t_btn = 0; 
        if (now - t_btn > 2000) { 
            t_btn = now; 
            Serial.println("[DEBUG] BOOT PIN IS READING LOW CONSTANTLY!"); 
        }
        lastActivityTime = now;
    }'''

replacement_loop_btn = '''    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        lastActivityTime = now;
        if (isScreenOn) {
            // Neu man hinh dang sang ma an BOOT -> Tat man hinh luon bang cach ep timeout!
            lastActivityTime = now - SCREEN_TIMEOUT_MS - 1000; 
        }
        while(digitalRead(BTN_PWR_PIN) == LOW) { delay(10); } // doi nha nut
    }'''

text = text.replace(target_loop_btn, replacement_loop_btn)

# 3. Add to light sleep block
target_sleep = '''        // Ngu nong khoang 100ms de tiet kiem pin, hoac bi thuc day boi UART som hon
        esp_sleep_enable_timer_wakeup(100000); 
        esp_light_sleep_start();'''

replacement_sleep = '''        // Cau hinh danh thuc bang nut bam ngay truoc khi ngu
        gpio_wakeup_enable((gpio_num_t)BTN_PWR_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();

        // Ngu nong khoang 100ms de tiet kiem pin, hoac bi thuc day boi UART som hon
        esp_sleep_enable_timer_wakeup(100000); 
        esp_light_sleep_start();
        
        // Vua tinh day, tat chuc nang wakeup tren chan nay de digitalRead hoat dong binh thuong
        gpio_wakeup_disable((gpio_num_t)BTN_PWR_PIN);'''

text = text.replace(target_sleep, replacement_sleep)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

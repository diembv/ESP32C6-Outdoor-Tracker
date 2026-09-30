import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# Remove the while loop that causes hanging
target_btn = '''    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        lastActivityTime = now;
        if (isScreenOn) {
            // Neu man hinh dang sang ma an BOOT -> Tat man hinh luon bang cach ep timeout!
            lastActivityTime = now - SCREEN_TIMEOUT_MS - 1000; 
        }
        while(digitalRead(BTN_PWR_PIN) == LOW) { delay(10); } // doi nha nut
    }'''

replacement_btn = '''    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    // Kiem tra debounce don gian
    static uint32_t last_btn_press = 0;
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        if (now - last_btn_press > 500) { // 500ms debounce
            last_btn_press = now;
            if (isScreenOn) {
                // Tat man hinh luon
                lastActivityTime = now - SCREEN_TIMEOUT_MS - 1000; 
            } else {
                // Bat man hinh
                lastActivityTime = now;
            }
        }
    }'''

text = text.replace(target_btn, replacement_btn)

# Properly remove gpio_wakeup_enable from setup()
# It was:
#    gpio_wakeup_enable((gpio_num_t)BTN_PWR_PIN, GPIO_INTR_LOW_LEVEL);
#    esp_sleep_enable_gpio_wakeup();
pattern_setup = re.compile(r'gpio_wakeup_enable\(\(gpio_num_t\)BTN_PWR_PIN.*?esp_sleep_enable_gpio_wakeup\(\);', re.DOTALL)
text = pattern_setup.sub('// Da xoa gpio_wakeup_enable khoi setup de digitalRead hoat dong!', text)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

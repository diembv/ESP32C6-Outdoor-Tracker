import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Fix the button debounce to use edge detection
target_btn = '''    // Kiem tra debounce don gian
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

replacement_btn = '''    // Kiem tra phat hien canh (Edge Detection) de tranh nhay lien tuc
    static bool last_btn_state = HIGH;
    bool current_btn_state = digitalRead(BTN_PWR_PIN);
    
    if (current_btn_state == LOW && last_btn_state == HIGH) {
        // Vua moi bam xuong
        if (isScreenOn) {
            // Tat man hinh luon
            lastActivityTime = now - SCREEN_TIMEOUT_MS - 1000; 
        } else {
            // Bat man hinh
            lastActivityTime = now;
        }
    }
    last_btn_state = current_btn_state;'''

text = text.replace(target_btn, replacement_btn)

# 2. Fix the pin configuration after sleep
target_sleep = '''        // Vua tinh day, tat chuc nang wakeup tren chan nay de digitalRead hoat dong binh thuong
        gpio_wakeup_disable((gpio_num_t)BTN_PWR_PIN);'''

replacement_sleep = '''        // Vua tinh day, tat chuc nang wakeup tren chan nay de digitalRead hoat dong binh thuong
        gpio_wakeup_disable((gpio_num_t)BTN_PWR_PIN);
        // BAT BUOC phai goi lai pinMode vi gpio_wakeup_enable da route chan nay sang RTC, lam digitalRead bi mu!
        pinMode(BTN_PWR_PIN, INPUT_PULLUP);'''

text = text.replace(target_sleep, replacement_sleep)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

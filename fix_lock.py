import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Lock around lv_disp_get_inactive_time
target_touch = '''    // ── 1. Đánh thức khi chạm màn hình ──
    if (lv_disp_get_inactive_time(NULL) < 100) {
        lastActivityTime = now;
    }'''

replacement_touch = '''    // ── 1. Đánh thức khi chạm màn hình ──
    if (example_lvgl_lock(-1)) {
        if (lv_disp_get_inactive_time(NULL) < 100) {
            lastActivityTime = now;
        }
        example_lvgl_unlock();
    }'''
text = text.replace(target_touch, replacement_touch)

# 2. Lock around lv_disp_trig_activity
target_trig = '''    else if (!isScreenOn && (now - lastActivityTime <= SCREEN_TIMEOUT_MS)) {
        isScreenOn = true;
        set_amoled_backlight(255); // Bat lai AMOLED
        
        lv_disp_trig_activity(NULL); // Tránh tắt lại ngay
    }'''

replacement_trig = '''    else if (!isScreenOn && (now - lastActivityTime <= SCREEN_TIMEOUT_MS)) {
        isScreenOn = true;
        set_amoled_backlight(255); // Bat lai AMOLED
        
        if (example_lvgl_lock(-1)) {
            lv_disp_trig_activity(NULL); // Tránh tắt lại ngay
            example_lvgl_unlock();
        }
    }'''
text = text.replace(target_trig, replacement_trig)

# 3. Fix button initial state to prevent instant trigger at boot
target_btn = '''    static bool last_btn_state = HIGH;
    bool current_btn_state = digitalRead(BTN_PWR_PIN);'''

replacement_btn = '''    static bool last_btn_state = digitalRead(BTN_PWR_PIN); // Khoi tao bang trang thai thuc te de tranh trigger luc boot
    bool current_btn_state = digitalRead(BTN_PWR_PIN);'''
text = text.replace(target_btn, replacement_btn)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = '''    if (current_btn_state == LOW && last_btn_state == HIGH) {
        // Vua moi bam xuong
        if (isScreenOn) {
            // Tat man hinh luon
            lastActivityTime = now - SCREEN_TIMEOUT_MS - 1000; 
        } else {
            // Bat man hinh
            lastActivityTime = now;
        }
    }'''

replacement = '''    if (current_btn_state == LOW && last_btn_state == HIGH) {
        Serial.println("[DEBUG] Nhan dien duoc nut BOOT duoc bam (Canh xuong)!");
        // Vua moi bam xuong
        if (isScreenOn) {
            Serial.println("[DEBUG] Ep tat man hinh luon!");
            // Tat man hinh luon
            lastActivityTime = now - SCREEN_TIMEOUT_MS - 1000; 
        } else {
            Serial.println("[DEBUG] Ep bat man hinh len!");
            // Bat man hinh
            lastActivityTime = now;
        }
    }'''

text = text.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

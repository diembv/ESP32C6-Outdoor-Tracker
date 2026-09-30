import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = '''    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        lastActivityTime = now;
        // Chờ nhả nút để tránh kích hoạt liên tục
        
    }'''

replacement = '''    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    // Kiem tra ky neu no luon LOW thi se chan timeout man hinh
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        static uint32_t t_btn = 0; 
        if (now - t_btn > 2000) { 
            t_btn = now; 
            Serial.println("[DEBUG] BOOT PIN IS READING LOW CONSTANTLY!"); 
        }
        lastActivityTime = now;
    }'''

if "[DEBUG] BOOT PIN IS READING LOW CONSTANTLY!" not in text:
    text = text.replace(target, replacement)

target2 = '''void setup() {
    Serial.begin(115200);
    delay(2000);'''

replacement2 = '''void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // FIX: Tranh treo khi rut cap USB CDC!
    delay(2000);'''

if "Serial.setTxTimeoutMs(0)" not in text:
    text = text.replace(target2, replacement2)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

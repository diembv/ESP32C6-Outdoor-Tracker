import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# Add button init to setup()
target_setup = '''void setup() {
    Serial.begin(115200);
    delay(2000);
    tBoot = millis();'''

replacement_setup = '''void setup() {
    Serial.begin(115200);
    delay(2000);
    tBoot = millis();
    
    // --- Power Button ---
    pinMode(BTN_PWR_PIN, INPUT_PULLUP);
    esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_PWR_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    gpio_wakeup_enable((gpio_num_t)BTN_PWR_PIN, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    
    // Kiem tra ly do thuc day
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("[PWR] Woke up from Deep Sleep by BOOT button!");
    }'''

if "--- Power Button ---" not in text:
    text = text.replace(target_setup, replacement_setup)

# Add button check to loop() wakeup conditions
target_loop_wake = '''    // ── 2. Đánh thức khi có rung lắc (Wake-on-motion) ─────────────────────
    if (g_snap.imu.valid) {
        if (abs(g_snap.imu.pitch_deg - lastPitch) > 15.0f || 
            abs(g_snap.imu.roll_deg - lastRoll) > 15.0f) {
            lastActivityTime = now; 
        }
        lastPitch = g_snap.imu.pitch_deg;
        lastRoll  = g_snap.imu.roll_deg;
    }'''

replacement_loop_wake = '''    // ── 2. Đánh thức khi có rung lắc (Wake-on-motion) ─────────────────────
    if (g_snap.imu.valid) {
        if (abs(g_snap.imu.pitch_deg - lastPitch) > 15.0f || 
            abs(g_snap.imu.roll_deg - lastRoll) > 15.0f) {
            lastActivityTime = now; 
        }
        lastPitch = g_snap.imu.pitch_deg;
        lastRoll  = g_snap.imu.roll_deg;
    }
    
    // ── 2.5 Đánh thức khi nhấn nút BOOT (GPIO9) ───────────────────────────
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        lastActivityTime = now;
    }'''

# Replace the text handling utf-8 block drawing chars by using regex because they might be different
pattern_wake = re.compile(r'if \(g_snap\.imu\.valid\) \{.*?lastRoll  = g_snap\.imu\.roll_deg;\s*\}', re.DOTALL)
match = pattern_wake.search(text)
if match and "digitalRead(BTN_PWR_PIN)" not in text:
    text = text[:match.end()] + '''
    
    // ── 2.5 Danh thuc khi nhan nut BOOT (GPIO9) ──
    if (digitalRead(BTN_PWR_PIN) == LOW) {
        lastActivityTime = now;
        // Chờ nhả nút để tránh kích hoạt liên tục
        while(digitalRead(BTN_PWR_PIN) == LOW) { delay(10); }
    }''' + text[match.end():]

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

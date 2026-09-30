import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = 'const uint32_t now = millis();'
replacement = '''const uint32_t now = millis();
    static uint32_t lpt=0;
    if(now-lpt>1000){ lpt=now; Serial.println("[DEBUG] Loop is running!"); }'''

if "[DEBUG] Loop is running!" not in text:
    text = text.replace(target, replacement)

# Let's also check if the UI is hanging because of LVGL lock!
# I will make sure the lock is released properly in printSerial!
target2 = '''    if (now - tLastPrint >= SERIAL_PRINT_MS) {
        tLastPrint = now;
        if (printCount++ % I2C_RETRY_EVERY == 0) {
            if (example_lvgl_lock(-1)) {
                if (!bmpOk) bmpOk = initBMP();
                if (!qmiOk) qmiOk = initQMI8658();
                example_lvgl_unlock();
            }
        }
        printSerial();
    }'''

replacement2 = '''    if (now - tLastPrint >= SERIAL_PRINT_MS) {
        tLastPrint = now;
        if (printCount++ % I2C_RETRY_EVERY == 0) {
            if (example_lvgl_lock(-1)) {
                if (!bmpOk) bmpOk = initBMP();
                if (!qmiOk) qmiOk = initQMI8658();
                example_lvgl_unlock();
            }
        }
        printSerial();
    }'''
# Wait, printSerial() doesn't take lock.

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

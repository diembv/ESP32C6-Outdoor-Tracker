import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = '''        // Cau hinh danh thuc bang nut bam ngay truoc khi ngu
        // Da xoa gpio_wakeup_enable khoi setup de digitalRead hoat dong!

        // Ngu nong khoang 100ms de tiet kiem pin, hoac bi thuc day boi UART som hon'''

replacement = '''        // Cau hinh danh thuc bang nut bam ngay truoc khi ngu
        gpio_wakeup_enable((gpio_num_t)BTN_PWR_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();

        // Ngu nong khoang 100ms de tiet kiem pin, hoac bi thuc day boi UART som hon'''

text = text.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

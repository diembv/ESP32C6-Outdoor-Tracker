import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Lock around lv_disp_get_inactive_time
pattern1 = re.compile(r'if \(lv_disp_get_inactive_time\(NULL\) < 100\)\s*\{\s*lastActivityTime = now;\s*\}')
text = pattern1.sub('if (example_lvgl_lock(-1)) {\n        if (lv_disp_get_inactive_time(NULL) < 100) {\n            lastActivityTime = now;\n        }\n        example_lvgl_unlock();\n    }', text)

# 2. Lock around lv_disp_trig_activity
pattern2 = re.compile(r'lv_disp_trig_activity\(NULL\);.*?\}', re.DOTALL)
text = pattern2.sub('if (example_lvgl_lock(-1)) {\n            lv_disp_trig_activity(NULL);\n            example_lvgl_unlock();\n        }\n    }', text, count=1)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

import sys
import os

# 1. Update main.cpp
main_path = r'd:\workspace\Venom\src\main.cpp'
if os.path.exists(main_path):
    with open(main_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    new_main = []
    for line in lines:
        if 'Wire.begin(38, 39' in line:
            new_main.append('    Wire.begin(1, 2, 400000); // Fixed for Grove SDA=1, SCL=2\n')
        elif 'M5.Log.printf(">>> [Physiology]' in line:
            new_main.append('        M5.Log.printf(">>> [Sensors] MIC:%.1f LUX:%.1f\\n", mic_level, lux);\n')
            new_main.append(line)
        else:
            new_main.append(line)
    with open(main_path, 'w', encoding='utf-8') as f:
        f.writelines(new_main)

# 2. Update Venom.cpp
v_path = r'd:\workspace\Venom\src\Venom.cpp'
if os.path.exists(v_path):
    with open(v_path, 'r', encoding='utf-8') as f:
        v_lines = f.readlines()
    
    # We need to find the range for drawEye precisely to avoid nested replacement issues
    def find_eye_blood_range(lines):
        for i in range(len(lines)):
            if 'uint16_t blood = 0x8800;' in lines[i]:
                return i
        return -1

    for i in range(len(v_lines)):
        if 'skeleton.setDynamicPhysics(0.005f, 1.15f);' in v_lines[i]:
            v_lines[i] = '                skeleton.setDynamicPhysics(0.18f, 1.05f);\n'
        if 'const float tiltFactor = 2.8f;' in v_lines[i]:
            v_lines[i] = '    const float tiltFactor = 1.8f;\n'
    
    idx = find_eye_blood_range(v_lines)
    if idx != -1:
        # Replace the bloodshot loop (usually 8-10 lines)
        # We look for the closing brace of the if (stress > 0.65f)
        v_lines[idx:idx+8] = [
            '        uint16_t blood = 0x8000;\n',
            '        for (int j = 0; j < 8; j++) {\n',
            '            float sang = j * 0.78f + (float)millis() * 0.001f;\n',
            '            float sr_out = 5.0f + (float)(random(30)) * 0.1f;\n',
            '            canvas->drawPixel((int)(eyeX + cosf(sang)*sr_out), (int)(eyeY + sinf(sang)*sr_out), blood);\n',
            '        }\n'
        ]

    with open(v_path, 'w', encoding='utf-8') as f:
        f.writelines(v_lines)
print('SUCCESS')

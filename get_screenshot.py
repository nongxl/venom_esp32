import serial
import serial.tools.list_ports
import base64
import os
import time
from datetime import datetime

def find_m5stack_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        desc = port.description.upper()
        if any(k in desc for k in ["CP210", "USB SERIAL", "USB DEVICE", "ESP32", "M5STACK", "JTAG"]):
            return port.device
        if port.vid == 0x303A:
            return port.device
    return "COM7" 

def listen_for_screenshots(target_port=None):
    port_name = target_port if target_port else find_m5stack_port()
    
    print(f"\n--- Venom 截屏监听服务 v6.4 (稳定版) ---")
    print(f"目标端口: {port_name}")
    print("[*] 提示: 烧录前请先按 Ctrl+C 关闭此脚本，烧录完成后再重新运行。\n")
    
    while True:
        ser = None
        try:
            ser = serial.Serial(port_name, 115200, timeout=1)
            print(f"✅ [{datetime.now().strftime('%H:%M:%S')}] 已成功连接到 {port_name}")
            
            collecting = False
            b64_buffer = ""
            
            while True:
                try:
                    line = ser.readline()
                except serial.SerialException:
                    print(f"\n⚠️ [{datetime.now().strftime('%H:%M:%S')}] 串口连接断开。")
                    break
                
                if not line:
                    continue
                    
                try:
                    raw_line = line.decode('utf-8', errors='ignore').strip()
                    
                    if "==VENOM_B64_START==" in raw_line:
                        print(f"\n🎯 [{datetime.now().strftime('%H:%M:%S')}] 匹配到标记，开始接收 Base64 流...")
                        collecting = True
                        b64_buffer = ""
                        continue
                    
                    if "==VENOM_B64_END==" in raw_line:
                        if collecting and b64_buffer:
                            print(f"[*] 接收完成 (长度: {len(b64_buffer)})，正在保存图片...")
                            try:
                                image_data = base64.b64decode(b64_buffer)
                                if not os.path.exists('screenshot'):
                                    os.makedirs('screenshot')
                                filename = f"screenshot/venom_{datetime.now().strftime('%H%M%S')}.png"
                                with open(filename, 'wb') as f:
                                    f.write(image_data)
                                abs_path = os.path.abspath(filename)
                                print(f"✅✅✅ 图片保存成功: {abs_path}\n")
                                
                                # 自动同步至 artifacts 目录以便 walkthrough 引用
                                dest_dir = r"C:\Users\666\.gemini\antigravity\brain\97ed3039-9108-4904-9c91-dd06c8ef59ac\.tempmediaStorage"
                                if not os.path.exists(dest_dir):
                                    os.makedirs(dest_dir)
                                import shutil
                                shutil.copy(filename, os.path.join(dest_dir, "media_97ed3039-9108-4904-9c91-dd06c8ef59ac_meniscus.png"))
                                shutil.copy(filename, os.path.join(os.path.dirname(dest_dir), "venom_meniscus_result.png"))
                            except Exception as decode_err:
                                print(f"!!! 解码失败: {decode_err}")
                        collecting = False
                        continue
                    
                    if collecting:
                        content = raw_line
                        if "]" in raw_line and raw_line.find("]") < 15:
                            content = raw_line.split("]", 1)[1]
                        b64_buffer += content.strip()
                    elif ">>>" in raw_line:
                        print(f"设备消息: {raw_line}")
                        
                except Exception:
                    pass
        
        except serial.SerialException:
            # 简化重连逻辑
            time.sleep(2)
            continue
        except KeyboardInterrupt:
            print("\n服务已通过用户请求停止。")
            if ser and ser.is_open: ser.close()
            return
        finally:
            if ser and ser.is_open: ser.close()

if __name__ == "__main__":
    listen_for_screenshots()

import struct

OF_channel = [
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # OF 0-9
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # OF 10-19
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  # OF 20-29
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1   # OF 30-39
]
Strip_channel = [0, 0, 0, 0, 0, 0, 0, 0]  # Strip 0-7的LED數量

VERSION_MAJOR = 1
VERSION_MINOR = 2
COLOR_MAP = {
    'g': (255, 0, 0),
    'r': (0, 255, 0),
    'b': (0, 0, 255),
    'rg': (255, 255, 0),
    'gb': (255, 0, 255),
    'rb': (0, 255, 255),
    'w': (255, 255, 255)
}

def calculate_checksum(frame_data):
    return sum(frame_data) & 0xFFFFFFFF

print("Enter breathing light settings:")
color_input = input("Color (g/r/b/rg/gb/rb/w): ").strip()
fade = int(input("Fade (0=off, 1=on): ") or "1")
time_interval = int(input("Time interval (ms): ") or "100")
frame_num = int(input("Frame number: ") or "100")

color = COLOR_MAP[color_input]
OF_NUM = sum(OF_channel)
STRIP_NUM = sum(1 for x in Strip_channel if x > 0)
total_leds = sum(Strip_channel)

# 生成 control.dat
with open("control.dat", "wb") as control_file:
    control_data = bytearray()

    control_data.extend(struct.pack('<BB', VERSION_MAJOR, VERSION_MINOR))
    
    for of_enabled in OF_channel:
        control_data.extend(struct.pack('<B', of_enabled))
    
    for strip_leds in Strip_channel:
        control_data.extend(struct.pack('<B', strip_leds))
    
    control_data.extend(struct.pack('<I', frame_num))
    
    for k in range(frame_num):
        timestamp = k * time_interval
        control_data.extend(struct.pack('<I', timestamp))
    
    checksum = calculate_checksum(control_data)
    
    control_file.write(control_data)
    control_file.write(struct.pack('<I', checksum))

# 生成 frame.dat
with open("frame.dat", "wb") as frame_file:
    frame_file.write(struct.pack('<BB', VERSION_MAJOR, VERSION_MINOR))
    
    for k in range(frame_num):
        frame_data = bytearray()
        start_time = k * time_interval
        frame_data.extend(struct.pack('<I', start_time))
        frame_data.append(fade)
        
        for i, enabled in enumerate(OF_channel):
            if enabled:
                if k % 2 == 1:
                    frame_data.append(color[0])  # G
                    frame_data.append(color[1])  # R
                    frame_data.append(color[2])  # B
                else:
                    frame_data.append(0)  # G
                    frame_data.append(0)  # R
                    frame_data.append(0)  # B
        
        for i, strip_leds in enumerate(Strip_channel):
            if strip_leds > 0:
                for j in range(strip_leds):
                    if k % 2 == 1:
                        frame_data.append(color[0])  # G
                        frame_data.append(color[1])  # R
                        frame_data.append(color[2])  # B
                    else:
                        frame_data.append(0)  # G
                        frame_data.append(0)  # R
                        frame_data.append(0)  # B
        
        checksum = calculate_checksum(frame_data)
        frame_file.write(frame_data)
        frame_file.write(struct.pack('<I', checksum))

print("Files generated.")
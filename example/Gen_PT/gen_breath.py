import struct
import argparse

# 硬體設定
OF_channel = [1]*40
Strip_channel = [100]*8
version_major = 1
version_minor = 2

COLOR_MAP = {
    'g': (255, 0, 0), 'r': (0, 255, 0), 'b': (0, 0, 255),
    'rg': (255, 255, 0), 'gb': (255, 0, 255), 'rb': (0, 255, 255),
    'w': (255, 255, 255)
}

def calculate_checksum(data):
    return sum(data) & 0xFFFFFFFF

def main():
    parser = argparse.ArgumentParser(description='Generate breathing light pattern files')
    parser.add_argument('-c', '--color', required=True, choices=COLOR_MAP.keys())
    parser.add_argument('-f', '--fade', type=int, choices=[0, 1], default=1)
    parser.add_argument('-i', '--interval', type=int, required=True)
    parser.add_argument('-t', '--total_time', type=int, required=True)
    
    args = parser.parse_args()
    
    color = COLOR_MAP[args.color]
    frame_num = (args.total_time + args.interval - 1) // args.interval
    
    # control.dat
    with open("control.dat", "wb") as f:
        data = bytearray()
        data.extend(struct.pack('<BB', version_major, version_minor))
        data.extend(struct.pack('B'*40, *OF_channel))
        data.extend(struct.pack('B'*8, *Strip_channel))
        data.extend(struct.pack('<I', frame_num))
        
        for i in range(frame_num):
            data.extend(struct.pack('<I', i * args.interval))
        
        checksum = calculate_checksum(data)
        f.write(data)
        f.write(struct.pack('<I', checksum))
    
    # frame.dat
    with open("frame.dat", "wb") as f:
        f.write(struct.pack('<BB', version_major, version_minor))
        
        for k in range(frame_num):
            frame = bytearray()
            frame.extend(struct.pack('<I', k * args.interval))
            frame.append(args.fade)
            
            # OF: 偶數幀亮色
            of_count = sum(OF_channel)
            for i in range(of_count):
                if k % 2 == 0:
                    frame.extend(color)
                else:
                    frame.extend((0,0,0))
            
            # Strip: 奇數幀亮色
            strip_count = sum(Strip_channel)
            for i in range(strip_count):
                if k % 2 == 1:
                    frame.extend(color)
                else:
                    frame.extend((0,0,0))
            
            f.write(frame)
            f.write(struct.pack('<I', calculate_checksum(frame)))
    
    print(f"Generated {frame_num} frames")

if __name__ == "__main__":
    main()
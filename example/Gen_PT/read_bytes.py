import struct

# 讀取 frame.dat 並顯示每3個字元一組
with open("frame.dat", "rb") as file:
    # 跳過前2個byte（版本號）
    file.read(2)
    
    count = 0
    print("Frame data (每3個bytes為一組):")
    print("-" * 30)
    
    while True:
        # 一次讀取3個bytes
        data = file.read(3)
        if not data:
            break
        
        # 如果讀取不足3個bytes，補0顯示
        if len(data) < 3:
            data += b'\x00' * (3 - len(data))
        
        # 顯示16進制和10進制
        hex_str = " ".join(f"{b:02X}" for b in data)
        dec_str = " ".join(f"{b:3d}" for b in data)
        
        print(f"[{count:4d}] {hex_str}  |  {dec_str}")
        count += 1

print(f"\n總共讀取 {count} 組（每組3 bytes）")
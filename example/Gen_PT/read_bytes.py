import struct

with open("control.dat", "rb") as file:
    count = 0
    print("-" * 30)
    
    while True:
        data = file.read(1)
        if not data:
            break
        
        hex_str = " ".join(f"{b:02X}" for b in data)
        dec_str = " ".join(f"{b:3d}" for b in data)
        
        print(f"[{count:4d}] {hex_str}  |  {dec_str}")
        count += 1

print(f"\nTotal read {count} bytes")
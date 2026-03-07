import serial, sys, time
ser = serial.Serial("COM5", 115200, timeout=0.1)
time.sleep(1)


for line in sys.stdin:
    cmd = line.strip()
    if not cmd:
        continue
    ser.write((cmd + "\n").encode())
    time.sleep(0.05)
    while ser.in_waiting:
        print(ser.readline().decode(errors="ignore"), end="")

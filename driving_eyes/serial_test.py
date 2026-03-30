#!/usr/bin/env python3
"""Reset ESP32, capture boot log, send test command."""
import serial, time, sys, glob

# Find port
ports = glob.glob('/dev/cu.usbmodem*')
if not ports:
    print("ERROR: No USB serial ports found")
    sys.exit(1)
PORT = ports[0]
print(f"Using port: {PORT}")

ser = serial.Serial(PORT, 115200, timeout=1)

# Reset via DTR/RTS
print("Resetting ESP32...")
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.1)
ser.setRTS(False)
time.sleep(0.1)
ser.setDTR(True)
time.sleep(0.5)
ser.reset_input_buffer()

# Capture boot output (15 seconds for full boot + animation loading)
print("=== Boot Output ===")
start = time.time()
while time.time() - start < 15:
    line = ser.readline()
    if line:
        try:
            text = line.decode('utf-8', errors='replace').rstrip()
            print(text)
        except:
            print(repr(line))

# Send 'L' for TURN_LEFT
print("\n=== Sending 'L' (TURN_LEFT) ===")
ser.write(b'L\n')
time.sleep(0.5)

# Read response
start = time.time()
while time.time() - start < 3:
    line = ser.readline()
    if line:
        try:
            print(line.decode('utf-8', errors='replace').rstrip())
        except:
            print(repr(line))

print("\n=== Done ===")
ser.close()

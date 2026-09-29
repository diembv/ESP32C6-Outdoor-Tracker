import serial
import time
print('Reading from COM6 and Resetting...')
ser = serial.Serial('COM6', 115200, timeout=1)
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.1)
ser.setDTR(True)
ser.setRTS(False)
end_time = time.time() + 8
while time.time() < end_time:
    line = ser.readline()
    if line:
        try:
            print(line.decode('utf-8', errors='replace').strip())
        except:
            pass
ser.close()

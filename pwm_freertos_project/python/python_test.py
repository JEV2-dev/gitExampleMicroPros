import serial
import time

# ==========================
# Configuración del serial
# ==========================
PORT = "/dev/ttyUSB0"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  # tiempo para que reseteé el ESP32

def send(cmd):
    print(f"Enviando: {cmd}")
    ser.write((cmd + "\n").encode())
    time.sleep(0.1)

# ==========================
# Secuencias de prueba
# ==========================
print("\n=== TEST AUTOMÁTICO PWM – ESP32 ===\n")

# CH1 sweep
send("SET CH1 000,025,050,075,100")

# CH2 reverse sweep
send("SET CH2 100,075,050,025,000")

# CH3 valores fijos repetidos
send("SET CH3 050,050,050,050")

print("\n=== SECUENCIA COMPLETA ===\n")

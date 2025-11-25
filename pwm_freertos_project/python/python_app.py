import serial
import time

PORT = "/dev/ttyUSB0"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(1)

print("\n=== CONTROL PWM INTERACTIVO – ESP32 ===\n")

while True:
    print("\nOpciones:")
    print("1) CH1")
    print("2) CH2")
    print("3) CH3")
    print("q) Salir")

    op = input("\nSelecciona canal: ")

    if op == "q":
        break

    if op == "1":
        ch = "CH1"
    elif op == "2":
        ch = "CH2"
    elif op == "3":
        ch = "CH3"
    else:
        print("Opción inválida.")
        continue

    vals = input("Ingresa duty (ej: 050 o 025,050,100): ")

    # validación básica
    if not all(c.isdigit() or c == "," for c in vals):
        print("Formato inválido. Solo números y comas.")
        continue

    cmd = f"SET {ch} {vals}"

    print(f"Enviando → {cmd}")
    ser.write((cmd + "\n").encode())

    time.sleep(0.1)

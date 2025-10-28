import csv
import time
import paramiko
from pathlib import Path
from datetime import datetime
from paho.mqtt import client as mqtt

# ---------- Configuración ----------
BROKER_HOST = "127.0.0.1"
BROKER_PORT = 1883
TOPICS = [
    "u01/sensors/temperature",
    "u01/sensors/gps",
    "u01/data/status",
    "u01/data/uptime"
]
CSV_PATH = Path("mqtt_capture.csv")

# ---------- Configuración SSH ----------
SSH_HOST = "127.0.0.1"
SSH_USER = "jaime"
CHECK_CMD = "systemctl is-active mosquitto"

# ---------- Función SSH ----------
def check_mosquitto_via_ssh():
    print("[SSH] Verificando servicio Mosquitto ...")
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(SSH_HOST, username=SSH_USER, timeout=10)
    _, stdout, _ = ssh.exec_command(CHECK_CMD, timeout=10)
    out = stdout.read().decode().strip()
    ssh.close()
    print(f"[SSH] Estado del servicio: {out}")

# ---------- Callbacks MQTT ----------
def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"[MQTT] Conectado (reason_code={reason_code}). Suscribiendo ...")
    for t in TOPICS:
        client.subscribe(t, qos=1)

def on_message(client, userdata, msg):
    ts = datetime.utcnow().isoformat()
    payload = msg.payload.decode(errors="replace")
    row = [ts, msg.topic, payload]
    print(f"[MQTT] {row}")
    write_row(row)

# ---------- Guardar CSV ----------
def write_row(row):
    new_file = not CSV_PATH.exists()
    with CSV_PATH.open("a", newline="") as f:
        w = csv.writer(f)
        if new_file:
            w.writerow(["timestamp_utc", "topic", "payload"])
        w.writerow(row)

# ---------- Programa principal ----------
def main():
    check_mosquitto_via_ssh()
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER_HOST, BROKER_PORT, keepalive=30)
    client.loop_start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[MQTT] Detenido por usuario.")
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()

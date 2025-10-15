#!/usr/bin/env bash
set -euo pipefail

# ---- Detectar SSID ----
SSID="$(nmcli -t -f active,ssid dev wifi 2>/dev/null | awk -F: '$1=="yes"{print $2; exit}' || true)"
if [ -z "${SSID:-}" ]; then
  SSID="$(iwgetid -r 2>/dev/null || true)"
fi
if [ -z "${SSID:-}" ]; then
  read -rp "SSID (nombre de tu WiFi): " SSID
fi

# ---- Obtener password (no se puede leer del sistema de forma portable/segura) ----
read -rsp "Password para '$SSID': " PASS; echo

# ---- IP del servidor (tu PC) ----
SERVER_IP="${1:-"10.7.4.2"}"

# ---- Generar defaults temporales ----
cat > sdkconfig.auto <<EOF
CONFIG_WIFI_SSID="${SSID}"
CONFIG_WIFI_PASSWORD="${PASS}"
CONFIG_TCP_SERVER_IP="${SERVER_IP}"
CONFIG_TCP_SERVER_PORT=3333
CONFIG_CAESAR_SHIFT=4
CONFIG_CAESAR_TEXT="Bren 123"
EOF
echo "sdkconfig.auto generado (SSID='${SSID}', SERVER_IP='${SERVER_IP}')."

# ---- Verificar idf.py disponible ----
if ! command -v idf.py >/dev/null 2>&1; then
  if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
    # shellcheck disable=SC1090
    . "$HOME/esp/esp-idf/export.sh"
  fi
fi
command -v idf.py >/dev/null 2>&1 || { echo "idf.py no disponible. Exporta ESP-IDF y reintenta."; exit 1; }

# ---- Build + Flash + Monitor ----
idf.py fullclean
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.auto" set-target esp32 build -v
PORT="${2:-/dev/ttyUSB0}"
idf.py -p "$PORT" flash monitor

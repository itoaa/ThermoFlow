#!/bin/bash
# flash.sh - Flash ThermoFlow to ESP32-S3

export IDF_PATH="${HOME}/esp-idf"
. "$IDF_PATH/export.sh"

cd "$(dirname "$0")"

# Auto-detect port or use default
PORT="${1:-/dev/ttyUSB0}"

echo "Flashing to $PORT..."
idf.py -p "$PORT" flash

echo ""
echo "Starting monitor..."
echo "Press Ctrl+] to exit"
idf.py -p "$PORT" monitor
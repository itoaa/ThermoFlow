#!/bin/bash
# build.sh - Build ThermoFlow with ESP-IDF

# Python setup
export PYTHON="$(which python3)"
export IDF_PYTHON="$PYTHON"

# ESP-IDF location
export IDF_PATH="${HOME}/esp-idf"

if [ ! -d "$IDF_PATH" ]; then
    echo "ERROR: ESP-IDF not found at $IDF_PATH"
    exit 1
fi

# Create python symlink in a local bin if needed
mkdir -p "$HOME/.local/bin"
if [ ! -f "$HOME/.local/bin/python" ]; then
    ln -sf "$PYTHON" "$HOME/.local/bin/python"
fi
export PATH="$HOME/.local/bin:$PATH"

# Export ESP-IDF environment
echo "Loading ESP-IDF from $IDF_PATH..."
. "$IDF_PATH/export.sh"

cd "$(dirname "$0")"

echo "=========================================="
echo "Building ThermoFlow for ESP32-S3"
echo "Python: $PYTHON"
echo "=========================================="

# Set target if not already set
if [ ! -f "sdkconfig" ]; then
    echo "Setting target to esp32s3..."
    idf.py set-target esp32s3
fi

# Build
echo "Building..."
idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "Build successful!"
    echo "Binary: build/thermoflow.bin"
    echo "=========================================="
else
    echo ""
    echo "=========================================="
    echo "Build failed!"
    echo "=========================================="
    exit 1
fi
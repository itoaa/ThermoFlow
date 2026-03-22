#!/bin/bash
# build_with_pio_idf.sh - Use PlatformIO's ESP-IDF to build ThermoFlow

# Find PlatformIO's ESP-IDF installation
PLATFORMIO_HOME="${HOME}/.platformio"
ESP_IDF_PATH="${PLATFORMIO_HOME}/packages/framework-espidf"
PYTHON_PATH="${PLATFORMIO_HOME}/penv/bin/python"

if [ ! -d "$ESP_IDF_PATH" ]; then
    echo "ERROR: ESP-IDF not found in PlatformIO packages"
    echo "Please run: pio run -e esp32-s3"
    echo "This will download ESP-IDF automatically"
    exit 1
fi

# Export ESP-IDF environment
export IDF_PATH="$ESP_IDF_PATH"
export IDF_PYTHON="$PYTHON_PATH"

# Add tools to PATH
export PATH="${ESP_IDF_PATH}/tools:${PLATFORMIO_HOME}/packages/toolchain-xtensa-esp32s3/bin:${PATH}"

# Create sdkconfig if it doesn't exist
if [ ! -f "sdkconfig" ]; then
    cp sdkconfig.defaults sdkconfig 2>/dev/null || echo "Using default config"
fi

echo "=========================================="
echo "ESP-IDF Path: $IDF_PATH"
echo "Python: $IDF_PYTHON"
echo "=========================================="

# Run idf.py with PlatformIO's Python
${IDF_PYTHON} -m idf \
    -C . \
    -DIDF_TARGET=esp32s3 \
    -DSDKCONFIG=sdkconfig \
    build

echo ""
echo "Build complete!"
echo "Binary location: build/esp32s3/thermoflow.bin"
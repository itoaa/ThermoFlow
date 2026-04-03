#!/bin/bash
# build.sh - Build ThermoFlow with ESP-IDF
# Updated for Mini-FTX extension

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Python setup
export PYTHON="$(which python3)"
export IDF_PYTHON="$PYTHON"

# ESP-IDF location
export IDF_PATH="${HOME}/esp-idf"

if [ ! -d "$IDF_PATH" ]; then
    echo -e "${RED}ERROR: ESP-IDF not found at $IDF_PATH${NC}"
    echo "Please install ESP-IDF:"
    echo "  cd ~"
    echo "  git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git"
    echo "  ./esp-idf/install.sh esp32s3"
    exit 1
fi

# Create python symlink in a local bin if needed
mkdir -p "$HOME/.local/bin"
if [ ! -f "$HOME/.local/bin/python" ]; then
    ln -sf "$PYTHON" "$HOME/.local/bin/python"
fi
export PATH="$HOME/.local/bin:$PATH"

# Export ESP-IDF environment
echo -e "${YELLOW}Loading ESP-IDF from $IDF_PATH...${NC}"
. "$IDF_PATH/export.sh"

cd "$(dirname "$0")"

echo "=========================================="
echo -e "${GREEN}Building ThermoFlow with Mini-FTX support${NC}"
echo "Python: $PYTHON"
echo "Target: ESP32-S3"
echo "=========================================="

# Check for required components
echo ""
echo "Checking components..."
REQUIRED_DIRS=(
    "components/heat_recovery"
    "components/mqtt_client"
    "components/web_server"
)

for dir in "${REQUIRED_DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        echo -e "${RED}ERROR: Missing component: $dir${NC}"
        exit 1
    fi
    echo "  ✓ $dir"
done

# Clean previous build if requested
if [ "$1" == "clean" ] || [ "$1" == "fullclean" ]; then
    echo ""
    echo -e "${YELLOW}Cleaning build...${NC}"
    idf.py fullclean
    echo -e "${GREEN}Clean complete${NC}"
fi

# Set target if not already set
if [ ! -f "sdkconfig" ]; then
    echo ""
    echo "Setting target to esp32s3..."
    idf.py set-target esp32s3
fi

# Build
echo ""
echo "=========================================="
echo "Building..."
echo "=========================================="
if idf.py build; then
    echo ""
    echo "=========================================="
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
    echo "Binaries:"
    echo "  - build/ThermoFlow.bin (main app)"
    echo "  - build/bootloader/bootloader.bin"
    echo "  - build/partition_table/partition-table.bin"
    echo ""
    echo "To flash: ./flash.sh [PORT]"
    echo "=========================================="
else
    echo ""
    echo "=========================================="
    echo -e "${RED}✗ Build failed!${NC}"
    echo ""
    echo "Common fixes:"
    echo "  - Run with 'clean' option: ./build.sh clean"
    echo "  - Check component dependencies in CMakeLists.txt"
    echo "  - Verify ESP-IDF installation"
    echo "=========================================="
    exit 1
fi

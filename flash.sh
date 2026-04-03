#!/bin/bash
# flash.sh - Flash ThermoFlow to ESP32-S3 with Mini-FTX support
# Usage: ./flash.sh [PORT]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

export IDF_PATH="${HOME}/esp-idf"

if [ ! -d "$IDF_PATH" ]; then
    echo -e "${RED}ERROR: ESP-IDF not found at $IDF_PATH${NC}"
    exit 1
fi

. "$IDF_PATH/export.sh"

cd "$(dirname "$0")"

# Check if build exists
if [ ! -f "build/ThermoFlow.bin" ]; then
    echo -e "${YELLOW}WARNING: No build found!${NC}"
    echo "Running build first..."
    ./build.sh || exit 1
fi

# Auto-detect port or use default
PORT="${1:-/dev/ttyUSB0}"

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo -e "${YELLOW}WARNING: Port $PORT not found${NC}"
    echo "Available ports:"
    ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  (none found)"
    echo ""
    echo -e "${BLUE}Please specify port:${NC}"
    echo "  ./flash.sh /dev/ttyUSB0"
    echo "  ./flash.sh /dev/ttyACM0"
    exit 1
fi

echo "=========================================="
echo -e "${GREEN}Flashing ThermoFlow with Mini-FTX${NC}"
echo "Port: $PORT"
echo "=========================================="
echo ""

# Flash
echo "Erasing and flashing..."
idf.py -p "$PORT" erase-flash || true
idf.py -p "$PORT" flash

echo ""
echo -e "${GREEN}✓ Flash successful!${NC}"
echo ""
echo "=========================================="
echo "Starting monitor..."
echo -e "${BLUE}Press Ctrl+] to exit${NC}"
echo "=========================================="
echo ""

# Start monitor
idf.py -p "$PORT" monitor

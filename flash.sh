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
    echo "Please install ESP-IDF first:"
    echo "  cd ~"
    echo "  git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git"
    echo "  ./esp-idf/install.sh esp32s3"
    exit 1
fi

. "$IDF_PATH/export.sh"

cd "$(dirname "$0")"

# Check for pre-built binaries in multiple locations
BINARY_LOCATIONS=(
    "build/ThermoFlow.bin"
    "binaries/ThermoFlow.bin"
)

BINARY_FOUND=""
for location in "${BINARY_LOCATIONS[@]}"; do
    if [ -f "$location" ]; then
        BINARY_FOUND="$location"
        break
    fi
done

if [ -z "$BINARY_FOUND" ]; then
    echo -e "${RED}ERROR: No pre-built binary found!${NC}"
    echo ""
    echo "Searched locations:"
    for location in "${BINARY_LOCATIONS[@]}"; do
        echo "  - $location"
    done
    echo ""
    echo -e "${YELLOW}Options:${NC}"
    echo "1. Download binaries from GitHub releases"
    echo "2. Build yourself: ./build.sh"
    echo ""
    exit 1
fi

echo -e "${GREEN}Found binary: $BINARY_FOUND${NC}"

# If binary is in binaries/, offer to use it directly or copy to build
if [[ "$BINARY_FOUND" == binaries/* ]]; then
    echo ""
    echo -e "${YELLOW}Binary is in binaries/ folder (pre-compiled)${NC}"
    echo "You can either:"
    echo "1. Flash directly from binaries/ (no build needed)"
    echo "2. Copy to build/ and flash normally"
    echo ""
    
    # Auto-select option 1 for simplicity
    echo "Using pre-built binary from binaries/"
    
    # Create build/ folder structure if needed
    mkdir -p build/bootloader build/partition_table
    
    # Copy binaries if they don't exist in build/
    if [ ! -f "build/ThermoFlow.bin" ]; then
        cp binaries/ThermoFlow.bin build/ 2>/dev/null || true
        cp binaries/bootloader.bin build/bootloader/ 2>/dev/null || true
        cp binaries/partition-table.bin build/partition_table/ 2>/dev/null || true
        echo -e "${GREEN}Copied binaries to build/ folder${NC}"
    fi
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
echo "Binary: $BINARY_FOUND"
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

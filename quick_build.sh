#!/bin/bash
# quick_build.sh - Quick build without full clean (for development)
# Usage: ./quick_build.sh

set -e

export IDF_PATH="${HOME}/esp-idf"
. "$IDF_PATH/export.sh"

cd "$(dirname "$0")"

echo "Quick build (incremental)..."
idf.py build

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Size:"
    ls -lh build/ThermoFlow.bin
else
    echo "Build failed!"
    exit 1
fi

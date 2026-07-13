#!/bin/bash
set -o pipefail

export SDKCONFIG_DEFAULTS=sdkconfig.defaults:sdkconfig.ci.defaults

idf.py build 2>&1 | tee build.log
status=${PIPESTATUS[0]}

if [ "$status" -ne 0 ]; then
  echo "=== BUILD FAILED (exit $status) ==="
  grep -E "error:|undefined reference|ninja: build stopped|fatal error|overflowed|will not fit" build.log | tail -n 40 || true
  echo "=== LOG TAIL ==="
  tail -n 80 build.log || true
  exit "$status"
fi
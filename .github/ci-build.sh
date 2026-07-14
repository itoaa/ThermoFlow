#!/bin/bash
set -o pipefail

export SDKCONFIG_DEFAULTS=sdkconfig.ci.defaults
export BUILD_NUMBER="${GITHUB_RUN_NUMBER:-0}"
export GIT_SHA="$(git rev-parse --short=7 HEAD 2>/dev/null || echo unknown)"
export CHANNEL="${THERMOFLOW_CHANNEL:-stable}"
export REVISION="${THERMOFLOW_REVISION:-1}"
export USE_BUILD_VERSION="${THERMOFLOW_USE_BUILD_VERSION:-1}"

python3 scripts/generate_version.py

idf.py build 2>&1 | tee build.log
status=${PIPESTATUS[0]}

if [ "$status" -ne 0 ]; then
  echo "=== BUILD FAILED (exit $status) ==="
  grep -E "error:|undefined reference|ninja: build stopped|fatal error|overflowed|will not fit" build.log | tail -n 40 || true
  echo "=== LOG TAIL ==="
  tail -n 80 build.log || true
  exit "$status"
fi
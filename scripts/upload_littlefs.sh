#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if command -v pio >/dev/null 2>&1; then
  pio run -e esp32dev -t uploadfs
elif command -v platformio >/dev/null 2>&1; then
  platformio run -e esp32dev -t uploadfs
else
  echo "PlatformIO CLI not found. Install with: pip install platformio"
  exit 1
fi

#!/usr/bin/env python3
import shutil
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmd = shutil.which("pio") or shutil.which("platformio")
if not cmd:
    print("PlatformIO CLI not found. Install with: pip install platformio")
    sys.exit(1)

result = subprocess.run([cmd, "run", "-e", "esp32dev", "-t", "uploadfs"], cwd=root)
sys.exit(result.returncode)

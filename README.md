# TinyPiOS (ESP32 DevKit V1)

TinyPiOS is a complete ESP32 web desktop project with OLED runtime status, terminal, file manager, editor, settings, OTA, and cookie-based sessions.

## Hardware

- Board: **ESP32 DevKit V1**
- Display: **SSD1306 OLED 128x64 (I2C)**
- I2C wiring:
  - **SDA -> GPIO23**
  - **SCL -> GPIO22**
  - **VCC -> 3V3**
  - **GND -> GND**

## Features

- Arduino framework + PlatformIO project
- LittleFS persistent storage
- ESPAsyncWebServer + AsyncTCP + ArduinoJson APIs
- OLED boot logo, loading bar, and live status
- STA auto-connect or AP fallback (`TinyPiOS` / `tinypios123`)
- Captive-portal-like redirect while in AP mode
- Login + cookie session persistence
- Salted+hashed password in LittleFS config
- Web dashboard, file manager, terminal, editor, settings, OTA
- WebSocket live telemetry and terminal updates

## Project Structure

- `platformio.ini`
- `src/main.cpp`
- `src/wifi.cpp`, `include/wifi.h`
- `src/terminal.cpp`, `include/terminal.h`
- `src/filesystem.cpp`, `include/filesystem.h`
- `src/oled.cpp`, `include/oled.h`
- `src/api.cpp`, `include/api.h`
- `src/editor.cpp`, `include/editor.h`
- `src/settings.cpp`, `include/settings.h`
- `data/web/index.html`
- `data/web/style.css`
- `data/web/app.js`
- `data/web/terminal.js`
- `data/web/editor.js`
- `data/web/icons/*.svg`
- `scripts/upload_littlefs.sh`
- `tools/upload_littlefs.py`

## Build & Flash (PlatformIO)

1. Install PlatformIO CLI:
   ```bash
   pip install platformio
   ```
2. Build firmware:
   ```bash
   pio run -e esp32dev
   ```
3. Flash firmware:
   ```bash
   pio run -e esp32dev -t upload
   ```
4. Open serial monitor:
   ```bash
   pio device monitor -b 115200
   ```

## LittleFS Upload Steps

Use any one of:

```bash
pio run -e esp32dev -t uploadfs
```

```bash
./scripts/upload_littlefs.sh
```

```bash
python tools/upload_littlefs.py
```

This uploads everything under `data/` (including `data/web/*`) to LittleFS.

## First-Time AP Setup Flow

If no Wi-Fi credentials are saved:

1. Device starts AP mode:
   - SSID: `TinyPiOS`
   - Password: `tinypios123`
2. Connect phone/laptop to this AP.
3. Open any URL (or `http://192.168.4.1`).
4. TinyPiOS UI loads; use Wi-Fi setup in settings/API.
5. Credentials are saved to LittleFS and device reconnects in STA mode.

## Login / Session Usage

- Open TinyPiOS web page in browser.
- Default login:
  - Username: `admin`
  - Password: `tinypios`
- Session cookie (`TPSID`) is used for persistence.
- Change password from Settings or terminal `passwd` command.

## OTA Firmware Update Usage

1. Build firmware (`pio run -e esp32dev`).
2. Open TinyPiOS -> **OTA** page.
3. Select `.bin` firmware file.
4. Click **Upload Firmware**.
5. Wait for progress bar to complete.
6. Device auto-reboots on success.

## REST API Endpoints

- `GET /api/system`
- `GET/POST /api/files`
- `POST /api/upload`
- `GET /api/download`
- `POST /api/reboot`
- `GET/POST /api/settings`
- `POST /api/terminal`
- `POST /api/editor/save`
- `GET /api/editor/load`
- `GET/POST /api/wifi`

Additional utility endpoints:
- `POST /api/login`
- `POST /api/logout`
- `GET /api/session`
- `POST /api/ota`

## Troubleshooting

### OLED not showing
- Confirm 3V3 power and GND.
- Confirm SDA=GPIO23 and SCL=GPIO22.
- Check OLED I2C address (`0x3C`) compatibility.

### Web UI not loading
- Ensure LittleFS files were uploaded (`uploadfs`).
- Check serial logs for LittleFS mount errors.

### Wi-Fi not connecting
- Re-enter credentials in settings.
- Verify router is 2.4GHz (ESP32 requires 2.4GHz).
- Reset credentials by factory reset or deleting `/config/settings.json` via serial tools.

### Login fails
- Ensure password was updated as expected.
- Reset to defaults (factory reset path) to restore `tinypios`.

### OTA fails
- Confirm selected file is valid ESP32 `.bin` firmware.
- Ensure stable power and network during upload.
- Reflash via USB if interrupted.

## Arduino IDE Compatibility Notes

This repository uses standard Arduino `.cpp/.h` modules and can be adapted to Arduino IDE by importing sources and web assets, but PlatformIO is the recommended workflow for reliable LittleFS + dependency management.

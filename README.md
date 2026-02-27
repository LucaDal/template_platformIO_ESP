# PlatformIO ESP Template

Starter project for ESP8266 and ESP32 boards using WiFiManager and ArduinoJson. It includes ready-to-use PlatformIO environments and optional flags for TLS and debug builds.

## Environments
- `esp8266` / `esp8266-ota`: ESP8266 builds (OTA variant uses the custom `scripts/upload_command_ota_script.py` uploader).
- `esp32-c3` / `esp32-c3-ota`: ESP32-C3 builds (OTA variant uses the same custom uploader).

Select the target with `-e` when running PlatformIO commands (examples below).

## Optional build flags
`platformio.ini` contains two commented flags under `build_flags`:
- `-DUSE_TLS`: enable TLS support in your firmware (uncomment to compile with TLS).
- `-DDEBUG`: enable debug code paths or logging guarded by `#ifdef DEBUG`.

Uncomment the lines (remove the leading `;`) in the `[env]` section to turn them on for all environments, or copy them into a specific environment to scope them.

## Common commands
- Build: `pio run -e esp8266` (replace with `esp8266-ota`, `esp32-c3`, or `esp32-c3-ota` as needed).
- Upload (serial): `pio run -e esp8266 -t upload`.
- Upload (OTA envs): `pio run -e esp8266-ota -t upload` (uses the custom OTA uploader script).
- Monitor: `pio device monitor -e esp8266`.

## OTA upload configuration
- Create a local `.env` file starting from `.env.example`.
- Set `PORTAL_SERVER_IP` and `DEVICE_TYPE_ID` in `.env`, or export `OTA_SERVER` and `OTA_DEVICE_TYPE_ID` in the shell before running `pio`.
- When you run `pio run -e esp8266-ota -t upload`, `scripts/upload_command_ota_script.py` loads those values and sends the firmware to the OTA endpoint.

## Libraries included
- **ArduinoJson**: JSON parsing and serialization.

Use these libraries directly in your firmware under `src/`, the dependencies are already declared in `platformio.ini`.

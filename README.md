# PlatformIO ESP Template

Arduino template for ESP8266 and ESP32-C3 projects. Includes Wi-Fi setup, LittleFS storage, verified TLS, portal configuration, MQTT, and OTA updates.

## Project layout

- `src/main.cpp`: application startup and main loop.
- `my-esp-libraries/`: reusable device libraries.
- `data/`: files included in the LittleFS image.
- `scripts/`: build helpers and the OTA server uploader.
- `platformio.ini`: boards, dependencies, and build settings.

## Build and upload

Install PlatformIO Core and initialize the Git submodule if needed:

```sh
git submodule update --init --recursive
pio run -e esp8266
pio run -e esp8266 -t upload
pio device monitor -e esp8266
```

Use `esp32-c3` for ESP32-C3 boards. Serial monitoring uses 115200 baud. Configure board settings and debug logging in `platformio.ini`.

## Device configuration

Copy `device.example.json` to `data/provisioning/device.json` and fill in the device values:

```json
{
  "version": 1,
  "portalServer": "example.com/api",
  "deviceCode": "DEVICE-01",
  "deviceTypeId": "sensor",
  "deviceSecret": "replace-with-a-unique-random-secret",
  "wifi_ssid": "ProjectSetup-A1B2"
}
```

All six fields are required. `wifi_ssid` is the setup access point name and accepts 1–32 UTF-8 bytes. The JSON file must not exceed 4096 bytes. Device configuration is excluded from Git.

Keep trusted root certificates in `data/provisioning/ca.crt`. The included bundle contains public ISRG roots; add any private server roots your deployment requires. The build script generates `certs.ar` for ESP8266 TLS verification.

Upload the filesystem on first installation:

```sh
pio run -e esp8266 -t uploadfs
```

**`uploadfs` replaces the device filesystem.** Include all files to retain in `data/`. Boards without automatic reset require bootloader entry before each serial upload. Firmware-only uploads preserve filesystem data.

## Runtime behavior

Startup validates provisioning, connects Wi-Fi, downloads application properties, checks OTA, and connects MQTT. Missing or invalid provisioning stops initialization. LittleFS mount failures never trigger automatic formatting.

If stored Wi-Fi credentials fail during startup, the device opens a password-protected setup access point. Its password is derived from the device secret; see [DeviceSetupManager](my-esp-libraries/DeviceSetupManager/README.md). Later Wi-Fi reconnections are handled by the platform core.

MQTT endpoint, credentials, and topics come from the portal. The client automatically subscribes to `subscribe` and `all` topics. Application code selects its own `publish` topics.

OTA checks run at startup and in the daily window configured in `src/main.cpp`. MQTT disconnects during HTTPS maintenance requests to release TLS memory. See [the libraries](my-esp-libraries/README.md) for API details.

## OTA server upload

Copy `.env.example` to `.env` and set `OTA_SERVER_ADDRESS`, `OTA_DEVICE_TYPE`, `OTA_EMAIL`, and `OTA_PASSWORD`. Use a server address such as `example.com/api`, without the scheme or trailing slash. `OTA_DEVICE_TYPE` is the target Device Type ID. Exported environment variables override `.env` values. The Python uploader requires `requests`.

```sh
pio run -e esp8266-ota -t upload
pio run -e esp32-c3-ota -t upload
```

ESP8266 firmware is compressed with gzip. ESP32-C3 firmware remains uncompressed. These commands publish firmware to the server; devices install it during OTA checks.

## Local checks

```sh
python3 -m unittest discover -s test -p 'test_*.py'
```

Host checks require Python `requests`, a C++ compiler, installed PlatformIO dependencies, and OpenSSL development headers. Tests simulate device network/storage operations and exercise the firmware libraries without flashing hardware.

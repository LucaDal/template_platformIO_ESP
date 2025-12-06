# MyDeviceProperties

Fetches device properties from your backend and caches them in EEPROM so they persist across reboots. Supports HTTP or HTTPS (controlled by the `USE_TLS` build flag).

## Features
- HTTP/HTTPS fetch from `http(s)://<server>/ota/<deviceTypeId>/properties`.
- Length-prefixed payload saved to EEPROM; skips writes if unchanged.
- Provides a `JsonDocument` in RAM for easy access.
- Compact debug logs when `DEBUG` is defined.

## Dependencies
- ArduinoJson (v7)
- WiFi/WiFiClientSecure (board core)

## Usage
```cpp
#include <Arduino.h>
#include "MyDeviceProperties.h"
#include "secret_data.h" // defines PORTAL_SERVER_IP, DEVICE_TYPE_ID

MyDeviceProperties props; // defaults: 512-byte EEPROM, last 3 bytes reserved

void setup() {
  Serial.begin(115200);
  // connect WiFi here...

  props.begin(PORTAL_SERVER_IP, DEVICE_TYPE_ID); // loads cached payload from EEPROM
  props.fetchAndStoreIfChanged();                // fetches and saves if different

  JsonDocument &doc = props.json();
  const char* name = doc["nome cubo"]["value"] | "";
  Serial.println(name);
}

void loop() {
  // Optionally poll again later:
  // props.fetchAndStoreIfChanged();
}
```

## HTTPS
- Build with `-DUSE_TLS`.
- Constructor argument `verifyCert=true` enforces CA validation (Let’s Encrypt roots from `Certs.h`).
- `verifyCert=false` keeps HTTPS transport but skips validation.

## Debug
- Uncomment `//#define DEBUG` in `MyDeviceProperties.h` (or add `-DDEBUG` to build flags) to enable `*MyProps:` logs.
- Leave disabled to avoid extra flash usage.

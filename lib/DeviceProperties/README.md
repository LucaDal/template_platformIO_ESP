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

  // Optional third argument sets EEPROM offset (defaults to 0 or ctor value)
  props.begin(PORTAL_SERVER_IP, DEVICE_TYPE_ID); 
  // props.begin(PORTAL_SERVER_IP, DEVICE_TYPE_ID, 16); // with explicit offset
  props.fetchAndStoreIfChanged();                // fetches and saves if different

  JsonDocument &doc = props.json();
  const char* propName = doc["key"] | "";
  Serial.println(propName);
}

void loop() {
  // Optionally poll again later:
  // props.fetchAndStoreIfChanged();
}
```

## HTTPS
- Build with `-DUSE_TLS` to enable HTTPS. Without it, only HTTP is used.
- Constructor argument `verifyCert=true` enables CA validation (Let’s Encrypt roots) on ESP8266/ESP32; `false` calls `setInsecure()` (lighter but no validation).

## Debug
- Uncomment `//#define DEBUG` in `MyDeviceProperties.h` (or add `-DDEBUG` to build flags) to enable `*MyProps:` logs.
- Leave disabled to avoid extra flash usage.

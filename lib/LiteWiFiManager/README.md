# LiteWiFiManager

A minimal Wi-Fi provisioning helper for ESP8266/ESP32 that reuses the core's native credential storage (no EEPROM/NVS code). It tries stored credentials first, otherwise opens a tiny access point with a single form to capture new SSID/password and saves them to the Wi-Fi stack.

## Usage

```cpp
#include "LiteWiFiManager.h"

LiteWiFiManager wifiProvision;

void setup() {
  Serial.begin(115200);
  wifiProvision.begin("MySetupAP"); // blocks until connected or portal timeout
}

void loop() {
  wifiProvision.loop(); // required while the portal is open
}
```

- Call `begin(apSsid, apPassword, timeoutMs, forcePortal)` to start.
- The portal stops after `timeoutMs` (0 = no timeout) or immediately after saving credentials.
- Stored credentials live in the Wi-Fi driver (same place `WiFi.begin()` persists them).

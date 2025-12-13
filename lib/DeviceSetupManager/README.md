# DeviceSetupManager

Tiny helper to store C strings (`const char*`) into EEPROM sequentially,
starting from a chosen offset and returning the next offset to continue
writing. Logs use `CommonDebug` (`*DSM:`) when `DEBUG` is defined.

## Features
- `begin()` wraps `EEPROM.begin()` with configurable size (default 512 bytes).
- `saveCString(offset, str)` writes length + bytes and returns the next offset
  (unchanged if it fails).
- `readCString(offset, &next)` reads back the string and optionally returns the
  following offset. Returns `nullptr` on error; pointer is valid until the next
  call.
- Reserves, by default, the last 3 bytes (handy if OTA versioning uses them)
  via `reservedTailBytes`.

## Usage
```cpp
#include "DeviceSetupManager.h"

DeviceSetupManager dsm(512, 3); // 512-byte EEPROM, last 3 bytes reserved

void setup() {
  Serial.begin(115200);
  dsm.begin();

  size_t offset = 0;
  offset = dsm.saveCString(offset, "first string");
  offset = dsm.saveCString(offset, "second");

  size_t next = 0;
  const char* first = dsm.readCString(0, &next);
  const char* second = dsm.readCString(next);
}
```

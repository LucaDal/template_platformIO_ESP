#ifndef DEVICE_SETUP_MANAGER_H
#define DEVICE_SETUP_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <CommonDebug.h>

#define DSM_LOG(msg) DBG_LOG("*DSM:", msg)
#define DSM_LOGF(fmt, ...) DBG_LOGF("*DSM:", fmt, ##__VA_ARGS__)

class DeviceSetupManager {
public:
  explicit DeviceSetupManager(size_t eepromSize = 512,
                              size_t reservedTailBytes = 3);
  ~DeviceSetupManager();

  bool begin();

  size_t saveCString(size_t startOffset, const char *value);
  const char *readCString(size_t startOffset, size_t *nextOffset = nullptr);

private:
  bool hasRoom(size_t startOffset, size_t needed) const;
  bool ensureBuffer(size_t len);

  size_t _eepromSize;
  size_t _reservedTailBytes;
  bool _begun{false};
  char *_readBuffer{nullptr};
  size_t _bufferLen{0};
};

#endif

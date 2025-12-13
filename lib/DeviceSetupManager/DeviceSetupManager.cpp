#include "DeviceSetupManager.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

DeviceSetupManager::DeviceSetupManager(size_t eepromSize,
                                       size_t reservedTailBytes)
    : _eepromSize(eepromSize), _reservedTailBytes(reservedTailBytes) {}

DeviceSetupManager::~DeviceSetupManager() {
  if (_readBuffer) {
    free(_readBuffer);
    _readBuffer = nullptr;
    _bufferLen = 0;
  }
}

bool DeviceSetupManager::begin() {
  if (_begun) {
    return true;
  }
#if defined(ESP8266)
  EEPROM.begin(_eepromSize);
  bool ok = true;
#else
  bool ok = EEPROM.begin(_eepromSize);
#endif
  if (!ok) {
    DSM_LOG("EEPROM begin failed");
    return false;
  }
  _begun = true;
  DSM_LOGF("EEPROM ready size=%u reserve=%u\n",
           static_cast<unsigned>(_eepromSize),
           static_cast<unsigned>(_reservedTailBytes));
  return true;
}

bool DeviceSetupManager::hasRoom(size_t startOffset, size_t needed) const {
  if (_eepromSize <= _reservedTailBytes) {
    return false;
  }
  size_t usable = _eepromSize - _reservedTailBytes;
  if (startOffset > usable) {
    return false;
  }
  return startOffset + needed <= usable;
}

size_t DeviceSetupManager::saveCString(size_t startOffset, const char *value) {
  if (!begin()) {
    return startOffset;
  }

  if (value == nullptr) {
    DSM_LOG("null string");
    return startOffset;
  }

  size_t len = strlen(value);
  if (len > UINT16_MAX) {
    DSM_LOG("string too long");
    return startOffset;
  }

  size_t needed = sizeof(uint16_t) + len;
  if (!hasRoom(startOffset, needed)) {
    DSM_LOG("not enough EEPROM space");
    return startOffset;
  }

  uint16_t len16 = static_cast<uint16_t>(len);
  EEPROM.write(startOffset, len16 & 0xFF);
  EEPROM.write(startOffset + 1, (len16 >> 8) & 0xFF);
  for (uint16_t i = 0; i < len16; i++) {
    EEPROM.write(startOffset + 2 + i, static_cast<uint8_t>(value[i]));
  }

  if (!EEPROM.commit()) {
    DSM_LOG("EEPROM commit failed");
    return startOffset;
  }

  size_t nextOffset = startOffset + 2 + len16;
  DSM_LOGF("saved %u bytes at %u next=%u\n", len16,
           static_cast<unsigned>(startOffset),
           static_cast<unsigned>(nextOffset));
  return nextOffset;
}

bool DeviceSetupManager::ensureBuffer(size_t len) {
  size_t needed = len + 1;  // +1 for terminator
  if (_bufferLen >= needed) {
    return true;
  }
  void *newBuf = realloc(_readBuffer, needed);
  if (!newBuf) {
    DSM_LOG("buffer alloc failed");
    return false;
  }
  _readBuffer = static_cast<char *>(newBuf);
  _bufferLen = needed;
  return true;
}

const char *DeviceSetupManager::readCString(size_t startOffset,
                                            size_t *nextOffset) {
  if (!_begun) {
    DSM_LOG("begin() not called");
    if (nextOffset) {
      *nextOffset = startOffset;
    }
    return nullptr;
  }

  if (_eepromSize <= _reservedTailBytes) {
    DSM_LOG("no usable EEPROM");
    if (nextOffset) {
      *nextOffset = startOffset;
    }
    return nullptr;
  }

  size_t usable = _eepromSize - _reservedTailBytes;
  if (startOffset + 1 >= usable) {
    DSM_LOG("offset out of range");
    if (nextOffset) {
      *nextOffset = startOffset;
    }
    return nullptr;
  }

  uint16_t len =
      EEPROM.read(startOffset) | (EEPROM.read(startOffset + 1) << 8);
  size_t end = startOffset + 2 + len;
  if (end > usable) {
    DSM_LOG("read length overflow");
    if (nextOffset) {
      *nextOffset = startOffset;
    }
    return nullptr;
  }

  if (!ensureBuffer(len)) {
    if (nextOffset) {
      *nextOffset = startOffset;
    }
    return nullptr;
  }
  for (uint16_t i = 0; i < len; i++) {
    _readBuffer[i] = static_cast<char>(EEPROM.read(startOffset + 2 + i));
  }
  _readBuffer[len] = '\0';

  if (nextOffset) {
    *nextOffset = end;
  }
  DSM_LOGF("read %u bytes from %u\n", len,
           static_cast<unsigned>(startOffset));
  return _readBuffer;
}

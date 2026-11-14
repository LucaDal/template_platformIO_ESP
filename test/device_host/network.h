#pragma once
#define NETWORK_H
#include "Arduino.h"
#include "MyFirmware.h"
struct Network {
  Network(const char *, bool) {}
  void WiFiBegin() {}
  bool isConnected() { return deviceMock::connected; }
  Firmware checkVersion(const String &, const String &) {
    return {deviceMock::serverVersion, "0123456789abcdef0123456789abcdef"};
  }
  bool fileDownload(const String &, const String &, const String &, const String &, const String &current) {
    ++deviceMock::downloads;
    deviceMock::downloadCurrentVersion = current;
    return false;
  }
};

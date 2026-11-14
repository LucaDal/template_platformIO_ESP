#pragma once
#include "stubs.h"
#include "MyFirmware.h"
struct FirmwareData {
  explicit FirmwareData(const char * = nullptr) {}
  String getNewFirmwareVersion() { return "1.0.0"; }
  String getOldFirmwareVersion() { return "1.0.0"; }
  String getFirmwareMD5Image() { return "checksum"; }
  void setNewFirmware(Firmware) {}
  bool hasNewUpdate() { return false; }
  bool saveVersion(String) { assert(false); return false; }
};
struct Network {
  Network(const char *, bool) {}
  void WiFiBegin() {}
  bool isConnected() { return mock::wifiConnected; }
  Firmware checkVersion(const String &, const String &) {
    ++mock::otaCalls;
    return {"1.0.0", "checksum"};
  }
  bool fileDownload(const String &, const String &, const String &, const String &, const String &) {
    assert(false);
    return false;
  }
};
struct FakeESP { void restart() { assert(false); } };
inline FakeESP ESP;

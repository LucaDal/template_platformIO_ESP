#pragma once
#include "Arduino.h"
struct IPAddress { String toString() const { return "192.168.4.1"; } };
struct FakeWiFi {
  void mode(int value) { deviceMock::wifiMode = value; }
  void setAutoConnect(bool) {}
  void setAutoReconnect(bool) {}
  void persistent(bool) {}
  void begin() { deviceMock::connected = deviceMock::storedConnects; }
  void begin(const char *ssid, const char *password) {
    deviceMock::savedSsid = ssid;
    deviceMock::savedPassword = password;
    deviceMock::connected = true;
  }
  int status() const { return deviceMock::connected ? WL_CONNECTED : 0; }
  bool softAP(const char *, const char *password) {
    ++deviceMock::apCalls;
    deviceMock::apPassword = password ? password : "";
    return deviceMock::apStarts;
  }
  IPAddress softAPIP() { return {}; }
  void softAPdisconnect(bool) {}
  int scanNetworks() { return deviceMock::networks.size(); }
  String SSID(int index) { return deviceMock::networks.at(index); }
  int RSSI(int) { return -40; }
  void scanDelete() {}
};
inline FakeWiFi WiFi;

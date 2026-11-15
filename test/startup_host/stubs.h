#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
using String = std::string;
#define F(value) value
#define PSTR(value) value

namespace mock {
inline unsigned long now = 0, wifiDuration = 0;
inline bool storageReady = true, provisioningReady = true, cached = true;
inline bool wifiShouldConnect = true, wifiConnected = false, fetchSuccess = true;
inline bool mqttReady = true, mqttConnected = false, otaReady = false, otaDue = false;
inline bool otaBecomesDueOnFetch = false;
inline bool scheduleReady = true;
inline int wifiCalls = 0, wifiAlive = 0, fetchCalls = 0, mqttCalls = 0, otaCalls = 0;
inline std::vector<std::string> events, messages;
}
inline unsigned long millis() { return mock::now; }
inline void delay(unsigned long value) { mock::now += value; }
struct FakeSerial {
  void begin(int baud) { assert(baud == 115200); mock::events.push_back("serial"); }
  void println(const char *message) { mock::messages.emplace_back(message); }
  template<class... Args> void printf(const char *format, Args... args) {
    char message[256];
    std::snprintf(message, sizeof(message), format, args...);
    mock::messages.emplace_back(message);
  }
};
inline FakeSerial Serial;
struct FakeWiFi { bool isConnected() { return mock::wifiConnected; } };
inline FakeWiFi WiFi;
struct DailySchedule {
  bool taken = false;
  DailySchedule(uint8_t, uint8_t, uint16_t) {}
  bool begin() { return mock::scheduleReady; }
  bool isDue(time_t) const { return mock::scheduleReady && mock::otaDue && !taken; }
  bool take(time_t now) {
    if (!isDue(now)) return false;
    taken = true;
    return true;
  }
  bool run(time_t now, void (*action)()) {
    if (!action || !take(now)) return false;
    action();
    return true;
  }
};
struct DeviceSetupManager {
  bool begin() { mock::events.push_back("provisioning"); return mock::storageReady; }
  bool isProvisioningReady() { return mock::provisioningReady; }
  const char *portalServerIp() { return "portal.test/api"; }
  const char *deviceCode() { return "device-code"; }
  const char *deviceTypeId() { return "sensor"; }
  const char *deviceSecret() { return "secret"; }
  String wifiApPassword() { return "test-ap-password"; }
  const char *wifiSsid() { return "Host Setup"; }
};
struct LiteWiFiManager {
  LiteWiFiManager() { ++mock::wifiAlive; }
  ~LiteWiFiManager() { --mock::wifiAlive; }
  bool begin(const char *ssid, const char *password) {
    assert(std::string(ssid) == "Host Setup");
    assert(std::string(password) == "test-ap-password");
    ++mock::wifiCalls;
    mock::events.push_back("wifi");
    mock::now += mock::wifiDuration;
    return mock::wifiConnected = mock::wifiShouldConnect;
  }
};
struct JsonObject {};
struct FakeDocument {
  bool available = false;
  template<class T> bool is() const { return available; }
};
struct MyDeviceProperties {
  FakeDocument document;
  bool begin(const char *, const char *, const char *) {
    mock::events.push_back("cache");
    return document.available = mock::cached;
  }
  bool fetchAndStoreIfChanged() {
    assert(mock::wifiConnected && !mock::mqttConnected);
    ++mock::fetchCalls;
    mock::events.push_back("properties.fetch");
    if (mock::otaBecomesDueOnFetch) mock::otaDue = true;
    if (mock::fetchSuccess) document.available = true;
    return mock::fetchSuccess;
  }
  FakeDocument &json() { return document; }
};
struct MQTTManager {
  enum class Error { None };
  struct ErrorInfo {
    Error code = Error::None;
    const char *message = "No error";
    int mqttError = 0, connack = 0, httpStatus = 0, tlsError = 0;
  };
  bool reconnect = true;
  void (*connectedCallback)() = nullptr;
  void (*disconnectedCallback)() = nullptr;
  void onConnected(void (*callback)()) { connectedCallback = callback; }
  void onDisconnected(void (*callback)()) { disconnectedCallback = callback; }
  void resume() { reconnect = true; }
  ErrorInfo lastError() { return {}; }
  template<class Callback> bool begin(const char *, const char *, const char *, Callback) {
    mock::events.push_back("mqtt.begin");
    return mock::mqttReady;
  }
  bool connected() { return mock::mqttConnected; }
  bool connect() {
    assert(mock::wifiConnected && mock::otaCalls > 0);
    ++mock::mqttCalls;
    mock::events.push_back("mqtt.connect");
    mock::mqttConnected = true;
    if (connectedCallback) connectedCallback();
    return true;
  }
  void disconnect() {
    reconnect = false;
    mock::events.push_back("mqtt.disconnect");
    if (mock::mqttConnected && disconnectedCallback) disconnectedCallback();
    mock::mqttConnected = false;
  }
  bool loop() {
    if (!mock::wifiConnected) {
      mock::events.push_back("mqtt.disconnect");
      mock::mqttConnected = false;
      return false;
    }
    if (!mock::mqttConnected && reconnect) return connect();
    return mock::mqttConnected;
  }
};
#ifndef TEST_REAL_OTA
struct SimpleOTA {
  void begin(const char *, const char *, const char *, const char *, bool) {
    mock::events.push_back("ota.begin");
    mock::otaReady = true;
  }
  bool isCheckDue(unsigned long) { return mock::otaReady && mock::otaDue; }
  bool checkUpdates(unsigned long seconds) {
    assert(mock::otaReady && mock::wifiConnected && !mock::mqttConnected);
    ++mock::otaCalls;
    mock::events.push_back("ota.check:" + std::to_string(seconds));
    return true;
  }
};
#endif

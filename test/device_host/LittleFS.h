#pragma once
#include "Arduino.h"

struct LittleFSConfig {
  bool autoFormat = true;
  void setAutoFormat(bool enabled) { autoFormat = enabled; }
};
struct File {
  std::string path;
  bool opened = false;
  size_t position = 0;
  explicit operator bool() const { return opened; }
  size_t size() const { return deviceMock::files.at(path).size(); }
  int read() {
    const auto &data = deviceMock::files.at(path);
    return position < data.size() ? static_cast<unsigned char>(data[position++]) : -1;
  }
  size_t readBytes(char *buffer, size_t count) {
    const auto &data = deviceMock::files.at(path);
    const auto length = std::min(count, data.size() - position);
    std::memcpy(buffer, data.data() + position, length);
    position += length;
    return length;
  }
  size_t write(uint8_t byte) { return write(&byte, 1); }
  size_t write(const uint8_t *buffer, size_t count) {
    const auto length = deviceMock::failWrite ? count / 2 : count;
    deviceMock::files[path].append(reinterpret_cast<const char *>(buffer), length);
    return length;
  }
  String readString() { return deviceMock::files.at(path); }
  String readStringUntil(char separator) { return deviceMock::files.at(path).substr(0, deviceMock::files.at(path).find(separator)); }
  size_t print(const String &value) {
    auto length = deviceMock::failWrite ? value.size() / 2 : value.size();
    deviceMock::files[path].append(value.data(), length);
    return length;
  }
  void close() { opened = false; }
};
struct FakeLittleFS {
  void setConfig(const LittleFSConfig &config) { deviceMock::autoFormat = config.autoFormat; }
  bool begin() { return begin(deviceMock::autoFormat); }
  bool begin(bool format) {
    deviceMock::formatRequested |= format;
    if (!deviceMock::fsMounted && format) deviceMock::files.clear();
    return deviceMock::fsMounted;
  }
  bool exists(const String &path) { return path == "/device" || path == "/provisioning" || deviceMock::files.count(path); }
  bool mkdir(const String &) { return true; }
  File open(const String &path, const char *mode) {
    if (std::string(mode) == "w") deviceMock::files[path].clear();
    if (!deviceMock::files.count(path)) return {};
    return {path, true};
  }
  bool rename(const String &from, const String &to) {
    if (deviceMock::failRename) return false;
    deviceMock::files[to] = deviceMock::files.at(from);
    deviceMock::files.erase(from);
    return true;
  }
  bool remove(const String &path) { return deviceMock::files.erase(path) != 0; }
};
inline FakeLittleFS LittleFS;

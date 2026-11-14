#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#define PROGMEM
#define F(value) value
#define PSTR(value) value

class String : public std::string {
public:
  using std::string::string;
  using std::string::operator=;
  String(const std::string &value) : std::string(value) {}
  bool isEmpty() const { return empty(); }
  size_t write(uint8_t c) { push_back(static_cast<char>(c)); return 1; }
  size_t write(const uint8_t *data, size_t length) { append(reinterpret_cast<const char *>(data), length); return length; }
  int lastIndexOf(char c) const { auto i = rfind(c); return i == npos ? -1 : static_cast<int>(i); }
  String substring(int begin, int end) const { return substr(begin, end - begin); }
  void trim() {
    auto start = find_first_not_of(" \t\r\n");
    if (start == npos) clear();
    else *this = substr(start, find_last_not_of(" \t\r\n") - start + 1);
  }
};

constexpr int HTTP_GET = 1, HTTP_POST = 2, HTTP_ANY = 3;
constexpr int WIFI_STA = 1, WIFI_AP = 2, WL_CONNECTED = 3;
namespace deviceMock {
struct Request { int method; std::string path; std::map<std::string, std::string> args; };
struct Response { int status; std::string body; };
inline unsigned long now = 0;
inline bool fsMounted = true, formatRequested = false, autoFormat = true;
inline bool failWrite = false, failRename = false;
inline std::map<std::string, std::string> files;
inline bool storedConnects = false, connected = false, apStarts = true;
inline int apCalls = 0, wifiMode = WIFI_STA;
inline String apPassword, savedSsid, savedPassword;
inline std::vector<std::string> networks;
inline std::deque<Request> requests;
inline std::vector<Response> responses;
inline String serverVersion = "1.2.3", downloadCurrentVersion;
inline int downloads = 0, reboots = 0;
}
inline unsigned long millis() { return deviceMock::now; }
inline void delay(unsigned long ms) { deviceMock::now += ms; }
class Print {};
inline Print Serial;
struct FakeESP { void restart() { ++deviceMock::reboots; } };
inline FakeESP ESP;

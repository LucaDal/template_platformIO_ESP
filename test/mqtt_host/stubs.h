#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using std::min;

class String : public std::string {
public:
  using std::string::string;
  String(const std::string &value) : std::string(value) {}
  bool isEmpty() const { return empty(); }
  bool endsWith(const char *suffix) const {
    std::string end(suffix);
    return size() >= end.size() && compare(size() - end.size(), end.size(), end) == 0;
  }
  void remove(size_t offset) { erase(offset); }
};

namespace mock {
struct Message { std::string topic, payload; bool retained; int qos; };
inline unsigned long now = 0;
inline bool tlsReady = true;
inline int httpStatus = 200, httpCalls = 0, mqttCalls = 0;
inline std::string response, url, failedSubscription;
inline std::map<std::string, std::string> headers;
inline std::vector<std::string> subscriptions;
inline std::vector<Message> messages;
inline void reset() {
  now = 0; tlsReady = true; httpStatus = 200; httpCalls = mqttCalls = 0;
  response.clear(); url.clear(); failedSubscription.clear();
  headers.clear(); subscriptions.clear(); messages.clear();
}
}

inline unsigned long millis() { return mock::now; }
struct TLSClient { bool httpActive = false; };
inline bool configureTLS(TLSClient &) { return mock::tlsReady; }
struct FakeWiFi { bool isConnected() { return true; } };
inline FakeWiFi WiFi;
constexpr int HTTP_CODE_OK = 200;

struct HTTPClient {
  bool http10 = false, reuse = true;
  TLSClient *transport = nullptr;
  std::istringstream body;
  void useHTTP10(bool value) { http10 = value; }
  void setReuse(bool value) { reuse = value; }
  bool begin(TLSClient &client, const String &url) {
    assert(http10 && !reuse);
    transport = &client;
    mock::url = url;
    body.str(mock::response);
    return true;
  }
  void addHeader(const char *name, const String &value) { mock::headers[name] = value; }
  int GET() { ++mock::httpCalls; transport->httpActive = true; return mock::httpStatus; }
  std::istringstream &getStream() { return body; }
  void end() { if (transport) transport->httpActive = false; }
};

struct MQTTClient {
  bool active = false;
  TLSClient *transport = nullptr;
  std::function<void(MQTTClient *, char *, char *, int)> callback;
  explicit MQTTClient(int size) { assert(size == 256); }
  template<class Callback> void onMessageAdvanced(Callback value) { callback = value; }
  void begin(const char *host, uint16_t port, TLSClient &client) {
    assert(std::string(host) == "mqtt.example.test" && port == 8883);
    transport = &client;
  }
  void setKeepAlive(uint16_t value) { assert(value == 30); }
  void setWill(const char *topic, const char *payload, bool retained, int qos) {
    assert(std::string(topic) == "devices/A/telemetry/availability");
    assert(std::string(payload) == "offline" && retained && qos == 1);
  }
  bool connect(const char *id, const char *user, const char *password) {
    assert(!transport->httpActive);
    assert(std::string(id) == "A" && std::string(user) == "dp-device:A");
    assert(std::string(password) == "secret");
    ++mock::mqttCalls;
    return active = true;
  }
  bool subscribe(const char *topic, int qos) {
    assert(qos == 1);
    mock::subscriptions.push_back(topic);
    return mock::failedSubscription != topic;
  }
  bool publish(const char *topic, const char *payload, bool retained, int qos) {
    if (!active) return false;
    mock::messages.push_back({topic, payload, retained, qos});
    return true;
  }
  bool connected() { return active; }
  bool disconnect() { active = false; return true; }
  bool loop() { return active; }
  int lastError() { return 0; }
};

#define DBG_LOGF(...)

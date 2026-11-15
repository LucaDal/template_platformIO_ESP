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
struct MQTTClient;
namespace mock { inline bool failStringWrites = false, failTopicAppend = false; }

class String : public std::string {
public:
  using std::string::string;
  String(const std::string &value) : std::string(value) {}
  size_t write(uint8_t value) { if (mock::failStringWrites) return 0; push_back(static_cast<char>(value)); return 1; }
  size_t write(const uint8_t *value, size_t length) {
    if (mock::failStringWrites) return 0;
    append(reinterpret_cast<const char *>(value), length);
    return length;
  }
  bool isEmpty() const { return empty(); }
  bool endsWith(const char *suffix) const {
    std::string end(suffix);
    return size() >= end.size() && compare(size() - end.size(), end.size(), end) == 0;
  }
  void remove(size_t offset) { erase(offset); }
  bool concat(const char *value) {
    if (mock::failTopicAppend) return false;
    append(value);
    return true;
  }
};

namespace mock {
struct Message { std::string topic, payload; bool retained; int qos; };
inline unsigned long now = 0;
inline bool tlsReady = true, wifiConnected = true, connectSuccess = true, publishSuccess = true;
inline bool loopSuccess = true, httpBegin = true, inCallback = false, incomingOnPublish = false;
inline int connack = 0, mqttError = 0, sslError = 0, readSize = 0, writeSize = 0;
inline MQTTClient *client = nullptr;
inline std::string incomingTopic, incomingPayload;
inline int httpStatus = 200, httpCalls = 0, mqttCalls = 0;
inline std::string response, url, failedSubscription;
inline std::map<std::string, std::string> headers;
inline std::vector<std::string> subscriptions;
inline std::vector<Message> messages;
inline void reset() {
  now = 0; tlsReady = wifiConnected = connectSuccess = publishSuccess = loopSuccess = httpBegin = true;
  inCallback = incomingOnPublish = failStringWrites = failTopicAppend = false;
  connack = mqttError = sslError = 0; client = nullptr; incomingTopic.clear(); incomingPayload.clear(); httpStatus = 200; httpCalls = mqttCalls = 0;
  response.clear(); url.clear(); failedSubscription.clear();
  headers.clear(); subscriptions.clear(); messages.clear();
}
}

inline unsigned long millis() { return mock::now; }
struct TLSClient { bool httpActive = false; int getLastSSLError() { return mock::sslError; } };
inline bool configureTLS(TLSClient &) { return mock::tlsReady; }
struct FakeWiFi { bool isConnected() { return mock::wifiConnected; } };
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
    return mock::httpBegin;
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
  MQTTClient(int receive, int transmit) { mock::readSize = receive; mock::writeSize = transmit; mock::client = this; }
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
    if (mock::connectSuccess) mock::connack = mock::mqttError = 0;
    return active = mock::connectSuccess;
  }
  bool subscribe(const char *topic, int qos) {
    assert(qos == 1);
    mock::subscriptions.push_back(topic);
    if (mock::failedSubscription == topic) { mock::mqttError = -11; return false; }
    return true;
  }
  bool publish(const char *topic, const char *payload, bool retained, int qos) {
    assert(!mock::inCallback);
    if (!active) return false;
    if (!mock::publishSuccess) { active = false; mock::mqttError = -4; return false; }
    const std::string originalTopic(topic), originalPayload(payload);
    mock::messages.push_back({topic, payload, retained, qos});
    if (mock::incomingOnPublish) receive();
    assert(originalTopic == topic && originalPayload == payload);
    return true;
  }
  bool connected() { return active; }
  bool disconnect() { active = false; mock::mqttError = 0; return true; }
  void receive() {
    if (mock::incomingTopic.empty()) return;
    auto topic = mock::incomingTopic, payload = mock::incomingPayload;
    mock::incomingTopic.clear(); mock::incomingPayload.clear();
    mock::inCallback = true;
    if (callback) callback(this, topic.data(), payload.data(), payload.size());
    mock::inCallback = false;
  }
  bool loop() {
    if (!mock::loopSuccess) { active = false; mock::mqttError = -13; return false; }
    if (!mock::incomingOnPublish) receive();
    return active;
  }
  int lastError() { return mock::mqttError; }
  int returnCode() { return mock::connack; }
};

#define LWMQTT_BAD_USERNAME_OR_PASSWORD 4
#define LWMQTT_NOT_AUTHORIZED 5
#define LWMQTT_CONNECTION_DENIED -10
#define DBG_LOGF(...)

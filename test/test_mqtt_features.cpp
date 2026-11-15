#include "MQTTManager.h"
#include <cassert>
#include <climits>

const char *configuration = R"({
  "schemaVersion":3,"protocol":"mqtts","host":"mqtt.example.test","port":8883,
  "username":"dp-device:A","password":"secret","clientId":"A","syncStatus":"synced","keepalive":30,
  "topics":{"publish":{"availability":"devices/A/telemetry/availability",
                         "state":"devices/A/state","telemetry":"devices/A/telemetry/#"},
            "subscribe":{"commands":"devices/A/commands/#"},"all":{}}
})";

MQTTManager *current = nullptr;
int connectedEvents = 0, disconnectedEvents = 0, received = 0;
bool publishInCallback = false, callbackState = false, publishOnConnected = false;
MQTTManager::ErrorInfo disconnectedError;

void onConnected() {
  ++connectedEvents;
  if (publishOnConnected) {
    JsonDocument document;
    document["outcome"] = callbackState;
    assert(current->publishJson("state", document, true));
  }
}
void onDisconnected() { ++disconnectedEvents; disconnectedError = current->lastError(); }
void receive(char *, uint8_t *, unsigned int) {
  ++received;
  if (publishInCallback) {
    const auto before = mock::messages.size();
    JsonDocument message;
    message["outcome"] = callbackState;
    assert(current->publishJson("state", message, true));
    assert(current->queuedMessages() > 0 && mock::messages.size() == before);
  }
}

void reset() {
  mock::reset();
  mock::response = configuration;
  connectedEvents = disconnectedEvents = received = 0;
  publishInCallback = callbackState = publishOnConnected = false;
}

void start(MQTTManager &mqtt) {
  current = &mqtt;
  mqtt.onConnected(onConnected);
  mqtt.onDisconnected(onDisconnected);
  assert(mqtt.begin("portal.test/api", "A", "secret", receive));
}

void testCacheAndLifecycle() {
  reset();
  MQTTManager mqtt;
  start(mqtt);
  assert(mqtt.loop() && connectedEvents == 1 && mock::httpCalls == 1);
  for (int index = 0; index < 100; ++index) assert(mqtt.loop());
  assert(mock::httpCalls == 1 && mock::mqttCalls == 1 && connectedEvents == 1);
  mqtt.disconnect();
  assert(disconnectedEvents == 1 && mock::messages.back().payload == "offline");
  for (int index = 0; index < 3; ++index) assert(!mqtt.loop());
  assert(mock::mqttCalls == 1);
  mqtt.resume();
  assert(mqtt.loop() && mock::httpCalls == 1 && mock::mqttCalls == 2 && connectedEvents == 2);
  mqtt.disconnect();
  mock::now = 300000;
  mqtt.resume();
  assert(mqtt.loop() && mock::httpCalls == 2);
  mqtt.disconnect();
  mock::connectSuccess = false;
  mock::connack = 5;
  mock::mqttError = -10;
  mqtt.resume();
  assert(!mqtt.loop() && mock::httpCalls == 2);
  assert(mqtt.lastError().code == MQTTManager::Error::Authentication);
  assert(mqtt.lastError().connack == 5 && mqtt.lastError().mqttError == -10);
  mock::now += 5000;
  mock::connectSuccess = true;
  assert(mqtt.loop() && mock::httpCalls == 3);
  mqtt.disconnect();
  mock::connectSuccess = false;
  mock::connack = 5;
  mock::mqttError = -3;
  mqtt.resume();
  assert(!mqtt.loop() && mqtt.lastError().code == MQTTManager::Error::Connection);
  mock::connectSuccess = true;
  mock::now += 5000;
  assert(mqtt.loop() && mock::httpCalls == 3);
}

void testBackoffAndRollover() {
  reset();
  MQTTManager mqtt;
  start(mqtt);
  mock::httpStatus = 503;
  assert(!mqtt.loop());
  assert(mqtt.lastError().httpStatus == 503 && mqtt.lastError().stage == MQTTManager::Stage::Provisioning);
  for (unsigned long delay : {5000UL, 10000UL, 20000UL, 40000UL, 60000UL, 60000UL}) {
    const auto requests = mock::httpCalls;
    mock::now += delay - 1;
    assert(!mqtt.loop() && mock::httpCalls == requests);
    ++mock::now;
    assert(!mqtt.loop() && mock::httpCalls == requests + 1);
  }
  mock::httpStatus = 200;
  mock::now += 60000;
  assert(mqtt.loop());
  mqtt.disconnect();
  mock::now = UINT32_MAX - 1000;
  mock::httpStatus = 503;
  mqtt.resume();
  assert(!mqtt.loop());
  const auto requests = mock::httpCalls;
  mock::now = 3998;
  assert(!mqtt.loop() && mock::httpCalls == requests);
  mock::now = 3999;
  assert(!mqtt.loop() && mock::httpCalls == requests + 1);
}

void testFailureDetails() {
  reset();
  MQTTManager mqtt;
  assert(!mqtt.loop() && mqtt.lastError().code == MQTTManager::Error::NotInitialized);
  start(mqtt);
  mock::tlsReady = false;
  mock::sslError = 62;
  assert(!mqtt.loop() && mqtt.lastError().code == MQTTManager::Error::TlsConfiguration);
#ifdef ESP8266
  assert(mqtt.lastError().tlsError == 62);
#endif
  mock::now = 5000;
  mock::tlsReady = true;
  mock::httpStatus = 401;
  assert(!mqtt.loop() && mqtt.lastError().httpStatus == 401);
  assert(mqtt.lastError().code == MQTTManager::Error::Authentication);
  mock::now += 10000;
  mock::httpStatus = 200;
  mock::response = "invalid JSON";
  assert(!mqtt.loop() && mqtt.lastError().code == MQTTManager::Error::InvalidConfiguration);
  mock::now += 20000;
  mock::response = configuration;
  mock::failedSubscription = "devices/A/commands/#";
  assert(!mqtt.loop() && mqtt.lastError().code == MQTTManager::Error::Subscription);
  assert(mqtt.lastError().mqttError == -11 && mock::mqttError == 0);
  mock::now += 40000;
  mock::failedSubscription.clear();
  assert(mqtt.loop());
  mock::loopSuccess = false;
  assert(!mqtt.loop() && disconnectedEvents == 1);
  assert(disconnectedError.code == MQTTManager::Error::ConnectionLost && disconnectedError.mqttError == -13);
  mqtt.disconnect();
  assert(mqtt.lastError().mqttError == -13 && mock::mqttError == 0);
  mock::loopSuccess = true;
  mock::now += 5000;
  mqtt.resume();
  assert(mqtt.loop());
  mock::publishSuccess = false;
  assert(!mqtt.publish("devices/A/state", "{}", true));
  assert(disconnectedError.stage == MQTTManager::Stage::Publication && disconnectedError.mqttError == -4);
  assert(mqtt.lastError().mqttError == -4);
}

void testJsonLimitsAndQos() {
  reset();
  MQTTManager::Options options;
  options.receiveBufferSize = 512;
  options.maxPayloadBytes = 32;
  MQTTManager mqtt(options);
  assert(mock::readSize == 512 && mock::writeSize == 256);
  start(mqtt);
  assert(mqtt.loop());
  JsonDocument document;
  document["outcome"] = false;
  assert(mqtt.publishJson("state", document, true));
  assert(mock::messages.back().topic == "devices/A/state" && mock::messages.back().payload == "{\"outcome\":false}");
  assert(mqtt.publishJson("telemetry", "state", document, false, 0));
  assert(mock::messages.back().qos == 0 && !mock::messages.back().retained);
  const auto messages = mock::messages.size();
  assert(!mqtt.publishJson("state", document, true, 2));
  assert(mqtt.lastError().code == MQTTManager::Error::InvalidQos);
  assert(!mqtt.publishJson("missing", document));
  assert(mqtt.lastError().code == MQTTManager::Error::InvalidTopic);
  assert(!mqtt.publishJson("telemetry", "bad/#", document));
  assert(!mqtt.publishJson("telemetry", "", document));
  assert(!mqtt.publish(std::string(300, 'x').c_str(), "{}"));
  assert(mqtt.lastError().code == MQTTManager::Error::PayloadTooLarge);
  mock::failStringWrites = true;
  assert(!mqtt.publishJson("state", document));
  assert(mqtt.lastError().code == MQTTManager::Error::OutOfMemory);
  mock::failStringWrites = false;
  document["long"] = "this is too long for the configured limit";
  assert(!mqtt.publishJson("state", document));
  assert(mqtt.lastError().code == MQTTManager::Error::PayloadTooLarge);
  assert(mock::messages.size() == messages && mqtt.connected());
  document.clear();
  document["outcome"] = false;
  mock::failTopicAppend = true;
  assert(mqtt.publishTopic("telemetry", "state").isEmpty());
  assert(!mqtt.publishJson("telemetry", "state", document));
  assert(mqtt.lastError().code == MQTTManager::Error::OutOfMemory);
  assert(mock::messages.size() == messages && mqtt.connected());
  assert(mqtt.enqueueJson("telemetry", "state", document));
  assert(mqtt.loop() && mqtt.queuedMessages() == 1);
  mock::failTopicAppend = false;
  assert(mqtt.loop() && mqtt.queuedMessages() == 0);
  mock::incomingTopic = "commands/test";
  mock::incomingPayload = std::string(33, 'x');
  assert(mqtt.loop() && received == 0);
  assert(mqtt.lastError().code == MQTTManager::Error::PayloadTooLarge);
}

void testQueueBoundsAndLifetime() {
  reset();
  MQTTManager mqtt;
  start(mqtt);
  JsonDocument document;
  document["outcome"] = false;
  assert(mqtt.enqueueJson("state", document, true));
  document["outcome"] = true;
  assert(mqtt.enqueueJson("state", document, true) && mqtt.queuedMessages() == 1);
  document.clear();
  assert(mqtt.loop() && mqtt.queuedMessages() == 0);
  assert(mock::messages.back().payload == "{\"outcome\":true}");
  for (int index = 0; index < 4; ++index) {
    document["value"] = index;
    assert(mqtt.enqueueJson("telemetry", "events", document));
  }
  assert(!mqtt.enqueueJson("state", document) && mqtt.lastError().code == MQTTManager::Error::QueueFull);
  for (int index = 0; index < 4; ++index) {
    assert(mqtt.loop());
    assert(mock::messages.back().payload == "{\"value\":" + std::to_string(index) + "}");
    assert(mqtt.queuedMessages() == 3 - index);
  }
  document.clear();
  document["outcome"] = false;
  assert(mqtt.enqueueJson("state", document, true));
  publishInCallback = callbackState = true;
  mock::incomingOnPublish = true;
  mock::incomingTopic = "devices/A/commands/state";
  mock::incomingPayload = "{}";
  assert(mqtt.loop() && mqtt.queuedMessages() == 1 && received == 1);
  assert(mock::messages.back().payload == "{\"outcome\":false}");
  assert(mqtt.loop() && mqtt.queuedMessages() == 0);
  assert(mock::messages.back().payload == "{\"outcome\":true}");
  document["outcome"] = false;
  assert(mqtt.enqueueJson("state", document, true));
  document["outcome"] = true;
  assert(mqtt.publishJson("state", document, true));
  assert(mqtt.queuedMessages() == 0);
  assert(mqtt.loop() && mock::messages.back().payload == "{\"outcome\":true}");
  mock::incomingOnPublish = false;
  mock::incomingTopic = "devices/A/commands/state";
  mock::incomingPayload = "{}";
  assert(mqtt.loop() && mqtt.queuedMessages() == 0 && received == 2);
  assert(mqtt.enqueueJson("missing", document));
  assert(mqtt.loop() && mqtt.queuedMessages() == 0 && mqtt.lastError().code == MQTTManager::Error::InvalidTopic);
  assert(mqtt.enqueueJson("state", document, true));
  mock::publishSuccess = false;
  assert(!mqtt.loop() && mqtt.queuedMessages() == 1);
  mock::publishSuccess = true;
  mock::now += 5000;
  assert(mqtt.loop() && mqtt.queuedMessages() == 0);
}

void testQueueFollowsNewConfiguration() {
  reset();
  MQTTManager::Options options;
  options.configurationTtlMs = 1000;
  MQTTManager mqtt(options);
  start(mqtt);
  assert(mqtt.loop());
  mqtt.disconnect();
  JsonDocument document;
  document["state"] = false;
  assert(mqtt.enqueueJson("state", document, true));
  const std::string oldTopic = "devices/A/state";
  mock::response.replace(mock::response.find(oldTopic), oldTopic.size(), "devices/A/new-state");
  mock::now = 1000;
  mqtt.resume();
  assert(mqtt.loop() && mock::messages.back().topic == "devices/A/new-state");
  assert(mock::httpCalls == 2 && mqtt.queuedMessages() == 0);
}

void testOptionValidationAndDocumentFailure() {
  reset();
  MQTTManager::Options options;
  options.queueCapacity = 5;
  MQTTManager invalid(options);
  assert(!invalid.begin("portal.test/api", "A", "secret"));
  assert(invalid.lastError().code == MQTTManager::Error::InvalidOptions);
  options.queueCapacity = 0;
  MQTTManager mqtt(options);
  start(mqtt);
  assert(mqtt.loop());
  JsonDocument document;
  document["state"] = false;
  assert(!mqtt.enqueueJson("state", document));
  assert(mqtt.lastError().code == MQTTManager::Error::QueueFull);
  assert(mqtt.publishJson("state", document));
  struct FailingAllocator : ArduinoJson::Allocator {
    void *allocate(size_t) override { return nullptr; }
    void deallocate(void *) override {}
    void *reallocate(void *, size_t) override { return nullptr; }
  } allocator;
  JsonDocument incomplete(&allocator);
  incomplete["missing"] = true;
  assert(incomplete.overflowed());
  assert(!mqtt.publishJson("state", incomplete));
  assert(mqtt.lastError().code == MQTTManager::Error::Serialization);
}

void testConnectionCallbackAndFullRetainedQueue() {
  reset();
  MQTTManager mqtt;
  start(mqtt);
  publishOnConnected = true;
  assert(mqtt.loop() && mqtt.queuedMessages() == 0);
  assert(mock::messages.back().topic == "devices/A/state" && mock::messages.back().payload == "{\"outcome\":false}");
  JsonDocument document;
  document["state"] = false;
  assert(mqtt.enqueueJson("state", document, true));
  for (int index = 0; index < 3; ++index) assert(mqtt.enqueueJson("telemetry", "event", document));
  document["state"] = true;
  assert(mqtt.enqueueJson("state", document, true) && mqtt.queuedMessages() == 4);
  assert(mqtt.loop() && mqtt.queuedMessages() == 3);
  assert(mock::messages.back().payload == "{\"state\":true}");
}

int main() {
  testCacheAndLifecycle();
  testBackoffAndRollover();
  testFailureDetails();
  testJsonLimitsAndQos();
  testQueueBoundsAndLifetime();
  testQueueFollowsNewConfiguration();
  testOptionValidationAndDocumentFailure();
  testConnectionCallbackAndFullRetainedQueue();
}

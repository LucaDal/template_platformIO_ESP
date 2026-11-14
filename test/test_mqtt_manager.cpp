#include "MQTTManager.h"
#include <cassert>

const char *configuration = R"({
  "schemaVersion":3,"protocol":"mqtts","host":"mqtt.example.test","port":8883,
  "username":"dp-device:A","password":"secret","clientId":"A","syncStatus":"synced","keepalive":30,
  "topics":{
    "publish":{"availability":"devices/A/telemetry/availability","telemetry":"devices/A/telemetry/#","state":"devices/A/state"},
    "subscribe":{"commands":"devices/A/commands/#","linked":"owners/1/+/state"},
    "all":{"shared":"owners/1/shared"}
  }
})";

void testUnwrapContent() {
  const struct { const char *message; const char *expected; } cases[] = {
    {R"({"email":"user@example.org","content":{"relay":true}})", R"({"relay":true})"},
    {R"({"relay":false,"temperature":0})", R"({"relay":false,"temperature":0})"},
    {R"({"email":"user@example.org","relay":true})", R"({"email":"user@example.org","relay":true})"},
    {R"({"content":null,"relay":true})", "null"},
    {R"({"content":false,"relay":true})", "false"},
    {R"({"content":0})", "0"},
    {R"({"content":""})", R"("")"},
    {R"({"content":{}})", "{}"},
    {R"({"content":[1,false,null]})", "[1,false,null]"},
    {R"({"content":{"content":{"relay":true}}})", R"({"content":{"relay":true}})"},
    {"{}", "{}"},
    {"[1,false,null]", "[1,false,null]"},
    {R"("online")", R"("online")"},
    {"false", "false"},
    {"0", "0"},
    {"null", "null"},
  };
  for (const auto &test : cases) {
    JsonDocument document;
    assert(!deserializeJson(document, test.message));
    JsonVariantConst content = MQTTManager::unwrapContent(document.as<JsonVariantConst>());
    std::string result;
    serializeJson(content, result);
    assert(result == test.expected);
    result.clear();
    serializeJson(document, result);
    assert(result == test.message);
  }

  JsonDocument document;
  assert(!deserializeJson(document, R"({"content":{"relay":true,"label":"fan"}})"));
  JsonVariantConst content = MQTTManager::unwrapContent(document.as<JsonVariantConst>());
  assert(content["label"].as<const char *>() == document["content"]["label"].as<const char *>());
  document["content"]["relay"] = false;
  assert(content["relay"].is<bool>() && !content["relay"].as<bool>());
  assert(MQTTManager::unwrapContent(JsonVariantConst()).isUnbound());
}

int main() {
  testUnwrapContent();
  mock::reset();
  mock::response = configuration;
  {
    MQTTManager mqtt;
    assert(!mqtt.begin("portal.test/api", "", "secret"));
    assert(mqtt.begin("portal.test/api", "A", "secret"));
    assert(mqtt.connect());
    assert(mock::url == "https://portal.test/api/device/mqtt-configuration");
    assert(mock::headers["x-device-code"] == "A" && mock::headers["x-device-secret"] == "secret");
    assert((mock::subscriptions == std::vector<std::string>{"devices/A/commands/#", "owners/1/+/state", "owners/1/shared"}));
    assert(mock::messages.size() == 1 && mock::messages.back().payload == "online");
    assert(mock::messages.back().retained && mock::messages.back().qos == 1);
    assert(mqtt.topics("publish").size() == 3 && mqtt.topics("all").size() == 1);
    assert(mqtt.topics(nullptr).isNull());
    assert(std::string(mqtt.publishTopic("shared")) == "owners/1/shared");
    assert(std::string(mqtt.publishTopic("state")) == "devices/A/state");
    assert(std::string(mqtt.publishTopic("commands")).empty());
    assert(std::string(mqtt.publishTopic("missing")).empty());
    assert(std::string(mqtt.publishTopic(nullptr)).empty());
    assert(mqtt.publishTopic("state") == mqtt.topics("publish")["state"].as<const char *>());
    assert(mqtt.publishTopic("telemetry", "measurements") == "devices/A/telemetry/measurements");
    assert(mqtt.publishTopic("telemetry", "relay/status") == "devices/A/telemetry/relay/status");
    assert(mqtt.publishTopic("telemetry", "bad/#").isEmpty());
    assert(mqtt.publishTopic("telemetry", "").isEmpty());
    assert(mqtt.publishTopic("telemetry", nullptr).isEmpty());
    assert(mqtt.publishTopic("state", "suffix").isEmpty());
    assert(!mqtt.publish(mqtt.publishTopic("telemetry"), "{}", true));
    assert(mqtt.publish(mqtt.publishTopic("state"), "{\"relay\":true}", true));
    assert(mock::messages.back().qos == 1 && mock::messages.back().retained);
    mqtt.disconnect();
    assert(mock::messages.back().payload == "offline" && !mqtt.connected());
    mock::subscriptions.clear();
    mock::response = R"({"schemaVersion":3,"protocol":"mqtts","host":"mqtt.example.test","port":8883,"username":"dp-device:A","password":"secret","clientId":"A","syncStatus":"synced","keepalive":30,"topics":{"publish":{"availability":"devices/A/telemetry/availability"},"subscribe":{"new":"devices/A/new/#"},"all":{}}})";
    assert(mqtt.connect());
    assert((mock::subscriptions == std::vector<std::string>{"devices/A/new/#"}));
    assert(std::string(mqtt.publishTopic("state")).empty());
    assert(mock::httpCalls == 2);
  }
  mock::reset();
  mock::response = configuration;
  {
    MQTTManager mqtt;
    assert(mqtt.begin("portal.test/api", "A", "secret"));
    mock::failedSubscription = "owners/1/shared";
    assert(!mqtt.connect() && !mqtt.connected());
    assert(mock::messages.empty());
    mock::now = 4999;
    assert(!mqtt.connect() && mock::httpCalls == 1);
    mock::failedSubscription.clear();
    mock::now = 5000;
    assert(mqtt.connect() && mock::httpCalls == 2);
  }
  mock::reset();
  {
    MQTTManager mqtt;
    assert(mqtt.begin("portal.test/api", "A", "secret"));
    mock::response = "invalid-json";
    assert(!mqtt.connect() && mock::mqttCalls == 0);
    mock::response = configuration;
    mock::now = 5000;
    assert(mqtt.connect());
  }
  mock::reset();
  {
    MQTTManager mqtt;
    assert(mqtt.begin("portal.test/api", "A", "secret"));
    mock::tlsReady = false;
    assert(!mqtt.connect() && mock::httpCalls == 0 && mock::mqttCalls == 0);
  }
}

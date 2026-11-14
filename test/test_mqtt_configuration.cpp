#include "MqttConfiguration.h"
#include <cassert>
#include <string>

int main() {
  const char *payload = R"({
    "schemaVersion":3,"protocol":"mqtts","host":"mqtt.example.test","port":8883,
    "username":"dp-device:A","password":"secret","clientId":"A",
    "syncStatus":"synced","keepalive":30,
    "topics":{
      "publish":{"availability":"devices/A/telemetry/availability","telemetry":"devices/A/telemetry/#"},
      "subscribe":{"commands":"devices/A/commands/#","linked":"owners/1/+/state"},
      "all":{"shared":"owners/1/shared/#"}
    }
  })";
  JsonDocument data;
  auto reset = [&]() { assert(!deserializeJson(data, payload)); };
  auto valid = [&]() { return validMqttConfiguration(data.as<JsonVariantConst>(), "A"); };
  reset();
  assert(valid());
  assert(!validMqttConfiguration(data.as<JsonVariantConst>(), "B"));
  assert(!validMqttConfiguration(data.as<JsonVariantConst>(), nullptr));

  for (const char *field : {"host", "username", "password", "clientId"}) {
    for (const char *value : {"", static_cast<const char *>(nullptr)}) {
      reset();
      data[field] = value;
      assert(!valid());
    }
    reset();
    data[field] = 123;
    assert(!valid());
    reset();
    data[field] = std::string("value\0suffix", 12);
    assert(!valid());
  }
  for (const char *field : {"port", "keepalive"}) {
    for (int value : {-1, 0, 65536}) {
      reset();
      data[field] = value;
      assert(!valid());
    }
    reset();
    data[field] = "8883";
    assert(!valid());
  }
  for (const char *action : {"publish", "subscribe", "all"}) {
    reset();
    data["topics"].remove(action);
    assert(!valid());
    reset();
    data["topics"][action] = "invalid";
    assert(!valid());
    reset();
    data["topics"][action]["bad"] = 123;
    assert(!valid());
    reset();
    data["topics"][action]["bad"] = "";
    assert(!valid());
    reset();
    data["topics"][action]["bad"] = "devices/A/#/state";
    assert(!valid());
    reset();
    data["topics"][action]["bad"] = std::string("topic\0suffix", 12);
    assert(!valid());
  }
  for (const char *topic : {"", "devices/+/availability", "devices/A/#"}) {
    reset();
    data["topics"]["publish"]["availability"] = topic;
    assert(!valid());
  }
  reset();
  data["topics"]["publish"].remove("availability");
  assert(!valid());
  for (const char *status : {"pending", "unknown"}) {
    reset();
    data["syncStatus"] = status;
    assert(!valid());
  }
  for (int version : {1, 2, 4}) {
    reset();
    data["schemaVersion"] = version;
    assert(!valid());
  }
  reset();
  data["protocol"] = "mqtt";
  assert(!valid());
  for (const char *host : {"https://mqtt.example.test", "mqtt.example.test/path", "mqtt example.test"}) {
    reset();
    data["host"] = host;
    assert(!valid());
  }
  reset();
  data["topics"]["subscribe"].as<JsonObject>().clear();
  data["topics"]["all"].as<JsonObject>().clear();
  data["topics"]["publish"].remove("telemetry");
  assert(valid());
  data["port"] = 65535;
  data["keepalive"] = 1;
  assert(valid());

  for (const char *topic : {"devices/A/state", "owners/1/shared"}) {
    assert(validMqttTopic(topic, false));
    assert(validMqttTopic(topic, true));
  }
  for (const char *topic : {"#", "+", "devices/A/#", "owners/+/state", "owners/+/+/#"}) {
    assert(validMqttTopic(topic, true));
    assert(!validMqttTopic(topic, false));
  }
  for (const char *topic : {"", "devices/A/#/state", "devices/A/state#", "owners/user+/state", "owners/+user/state"}) {
    assert(!validMqttTopic(topic, true));
    assert(!validMqttTopic(topic, false));
  }
  assert(!validMqttTopic(nullptr, true));
  assert(!validMqttTopic(std::string(65536, 'x').c_str(), true));
}

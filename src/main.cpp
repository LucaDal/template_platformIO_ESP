#include "DeviceSetupManager.h"
#include "LiteWiFiManager.h"
#include "MQTTManager.h"
#include "MyDeviceProperties.h"
#include "SimpleOTA.h"
#include <CommonDebug.h>
#include <Arduino.h>

#define LOG(fmt, ...) DBG_LOGF("*MAIN:", fmt, ##__VA_ARGS__)

SimpleOTA *simpleOTA = new SimpleOTA();
MyDeviceProperties deviceProperties;
LiteWiFiManager wifiProvision;
DeviceSetupManager setupMgr;
MQTTManager mqttManager;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  LOG("mqtt topic=%s len=%u\n", topic, length);
}

void connectToMQTT() {
  const char *mqttBroker = deviceProperties.Get("MQTT_BROKER");
  const char *mqttTopic = deviceProperties.Get("topic");
  if (strlen(mqttBroker) == 0 || strlen(mqttTopic) == 0) {
    return;
  }

  String clientId = "esp-client-" + String(WiFi.macAddress());
  if (mqttManager.connect(clientId.c_str())) {
    LOG("Connected to MQTT broker\n");
    mqttManager.subscribe(mqttTopic);
  } else {
    LOG("MQTT connect failed, rc=%d\n", mqttManager.state());
  }
}

void setup() {
  Serial.begin(115200);
  wifiProvision.begin("ProjectSetup");

  if (!setupMgr.begin()) {
    LOG("DeviceSetupManager begin failed\n");
    return;
  }

  if (WiFi.status() == WL_CONNECTED &&
      strlen(setupMgr.deviceId()) > 0 &&
      strlen(setupMgr.deviceSecret()) > 0 &&
      strlen(setupMgr.deviceTypeId()) > 0 &&
      strlen(setupMgr.portalServerIp()) > 0) {
    deviceProperties.begin(setupMgr.portalServerIp(), setupMgr.deviceId(),
                           setupMgr.deviceSecret());
    deviceProperties.fetchAndStoreIfChanged();
    simpleOTA->begin(setupMgr.portalServerIp(), setupMgr.deviceTypeId(),
                     setupMgr.deviceId(), setupMgr.deviceSecret(), true);
    if (mqttManager.begin(deviceProperties.Get("MQTT_BROKER"),
                          static_cast<uint16_t>(deviceProperties.GetInt("MQTT_PORT", 8883)),
                          mqttCallback)) {
      connectToMQTT();
    }
  }
}

void loop() {
  wifiProvision.loop();
  simpleOTA->checkUpdates(300);
  if (!mqttManager.connected()) {
    connectToMQTT();
  }
  mqttManager.loop();
  const char *value = deviceProperties.Get("key");
  LOG("value=%s\n", value);
  delay(5000);
}

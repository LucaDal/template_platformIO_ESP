#include "DeviceSetupManager.h"
#include "LiteWiFiManager.h"
#include "MQTTManager.h"
#include "MyDeviceProperties.h"
#include "SimpleOTA.h"
#include <Arduino.h>
#include <CommonDebug.h>

#define LOG(fmt, ...) DBG_LOGF("*MAIN:", fmt, ##__VA_ARGS__)

SimpleOTA *simpleOTA = new SimpleOTA();
MyDeviceProperties deviceProperties;
LiteWiFiManager wifiProvision;
DeviceSetupManager setupMgr;
MQTTManager mqttManager;

const char *mqttBroker;
const char *mqttTopic;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    LOG("mqtt topic=%s len=%u\n", topic, length);
}

void connectToMQTT() {
    const char *mqttBroker = deviceProperties.Get("MQTT_BROKER");
    const char *mqttTopic = deviceProperties.Get("topic");
    if (strlen(mqttBroker) == 0 || strlen(mqttTopic) == 0) {
        return;
    }
    if (mqttManager.connect(setupMgr.deviceId())) {
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

    if (setupMgr.isProvisioningReady() > 0) {
        deviceProperties.begin(setupMgr.portalServerIp(), setupMgr.deviceId(),
                               setupMgr.deviceSecret());
        deviceProperties.fetchAndStoreIfChanged();
        simpleOTA->begin(setupMgr.portalServerIp(), setupMgr.deviceTypeId(),
                         setupMgr.deviceId(), setupMgr.deviceSecret(), true);

        mqttBroker = deviceProperties.Get("MQTT_BROKER");
        mqttTopic = deviceProperties.Get("topic");
        uint16_t port =
            static_cast<uint16_t>(deviceProperties.GetInt("MQTT_PORT", 8884));
        if (mqttManager.begin(mqttBroker, port, mqttCallback)) {
            connectToMQTT();
        }
    }
}

void loop() {
    simpleOTA->checkUpdates(300);
    if (!mqttManager.connected()) {
        connectToMQTT();
    }
    mqttManager.loop();
    const char *value = deviceProperties.Get("key");
    LOG("value=%s\n", value);
    delay(5000);
}

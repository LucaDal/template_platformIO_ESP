#include "DailySchedule.h"
#include "DeviceSetupManager.h"
#include "LiteWiFiManager.h"
#include "MQTTManager.h"
#include "MyDeviceProperties.h"
#include "SimpleOTA.h"
#include <Arduino.h>
#include <CommonDebug.h>
#include <time.h>

#define LOG(fmt, ...) DBG_LOGF("*MAIN:", fmt, ##__VA_ARGS__)

constexpr unsigned long RETRY_INTERVAL_MS = 60000;
constexpr uint8_t OTA_HOUR = 3;
constexpr uint8_t OTA_MINUTE = 0;
constexpr uint16_t OTA_WINDOW_MINUTES = 15;

SimpleOTA simpleOTA;
DailySchedule otaSchedule(OTA_HOUR, OTA_MINUTE, OTA_WINDOW_MINUTES);
MyDeviceProperties deviceProperties;
DeviceSetupManager setupMgr;
MQTTManager mqttManager;

bool initialized = false;
bool wifiConnectedOnce = false;
bool initialOtaCheckPending = true;
bool propertiesFetched = false;
unsigned long lastWiFiAttempt = 0;
unsigned long lastPropertiesAttempt = 0;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    LOG("mqtt topic=%s len=%u\n", topic, length);
}

void connectToWiFi() {
    LiteWiFiManager wifiProvision;
    const String apPassword = setupMgr.wifiApPassword();
    if (!wifiProvision.begin(setupMgr.wifiSsid(), apPassword.c_str())) {
        LOG("Wi-Fi unavailable; retrying in %lu seconds.\n", RETRY_INTERVAL_MS / 1000UL);
    } else {
        wifiConnectedOnce = true;
    }
    lastWiFiAttempt = millis();
}

bool initializeDevice() {
    if (!setupMgr.begin()) {
        LOG("Provisioning storage unavailable.");
        return false;
    }
    if (!setupMgr.isProvisioningReady()) {
        LOG("Provisioning incomplete; check the device files.");
        return false;
    }

    if (!deviceProperties.begin(setupMgr.portalServerIp(), setupMgr.deviceCode(), setupMgr.deviceSecret())) {
        LOG("Properties cache unavailable; download needed.");
    }
    simpleOTA.begin(
        setupMgr.portalServerIp(), setupMgr.deviceTypeId(), setupMgr.deviceCode(), setupMgr.deviceSecret(), true);
    if (!otaSchedule.begin()) {
        LOG("Invalid OTA schedule; scheduled checks disabled.\n");
    }
    if (!mqttManager.begin(setupMgr.portalServerIp(), setupMgr.deviceCode(), setupMgr.deviceSecret(), mqttCallback)) {
        LOG("MQTT initialization failed.");
        return false;
    }
    return true;
}

bool ensureWiFi() {
    if (WiFi.isConnected()) {
        wifiConnectedOnce = true;
        return true;
    }
    mqttManager.disconnect();
    if (!wifiConnectedOnce && millis() - lastWiFiAttempt >= RETRY_INTERVAL_MS) {
        connectToWiFi();
    }
    return false;
}

void updatePortalServices() {
    const bool propertiesDue =
        !propertiesFetched && (initialOtaCheckPending || millis() - lastPropertiesAttempt >= RETRY_INTERVAL_MS);
    if (!propertiesDue && !initialOtaCheckPending && !otaSchedule.isDue(time(nullptr))) {
        return;
    }

    mqttManager.disconnect();
    if (propertiesDue) {
        propertiesFetched = deviceProperties.fetchAndStoreIfChanged();
        lastPropertiesAttempt = millis();
        if (!propertiesFetched) {
            if (deviceProperties.json().is<JsonObject>()) {
                LOG("Properties download failed; using cached values. Retrying in %lu seconds.\n",
                    RETRY_INTERVAL_MS / 1000UL);
            } else {
                LOG("Properties unavailable; retrying in %lu seconds.\n", RETRY_INTERVAL_MS / 1000UL);
            }
        }
    }
    const auto checkOta = [] { simpleOTA.checkUpdates(0); };
    if (initialOtaCheckPending) {
        initialOtaCheckPending = false;
        checkOta();
        // Count a startup check in today's window, including when it synchronized the clock.
        otaSchedule.take(time(nullptr));
    } else {
        otaSchedule.run(time(nullptr), checkOta);
    }
}

void setup() {
#ifdef DEBUG
    Serial.begin(115200);
#endif
    initialized = initializeDevice();
    if (initialized) {
        connectToWiFi();
    }
}

void loop() {
    if (!initialized || !ensureWiFi()) {
        delay(10);
        return;
    }

    updatePortalServices();
    if (!mqttManager.connected() && mqttManager.connect()) {
        LOG("Connected to MQTT broker\n");
    }
    mqttManager.loop();
#ifdef ESP32
    // only for esp, to give space to its routines
    delay(1);
#endif
}

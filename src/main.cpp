#include "DeviceSetupManager.h"
#include "LiteWiFiManager.h"
#include "MyDeviceProperties.h"
#include "SimpleOTA.h"
#include <CommonDebug.h>
#include "secret_data.h"
#include <Arduino.h>

#define LOG(fmt, ...) DBG_LOGF("*MAIN:", fmt, ##__VA_ARGS__)

SimpleOTA *simpleOTA = new SimpleOTA();
MyDeviceProperties deviceProperties;
LiteWiFiManager wifiProvision;
DeviceSetupManager setupMgr;
String deviceId;

void setup() {
  Serial.begin(115200);
  wifiProvision.begin("ProjectSetup");

  if (!setupMgr.begin()) {
    LOG("DeviceSetupManager begin failed\n");
  } else {
    deviceId = setupMgr.readDeviceId();
    if (deviceId.isEmpty()) {
      LOG("Device ID not settled. please provide one.\n");
    }
  }

  LOG("DEVICE ID [%s]\n", deviceId.c_str());

  if (WiFi.status() == WL_CONNECTED && !deviceId.isEmpty()) {
    deviceProperties.begin(PORTAL_SERVER_IP, deviceId.c_str());
    deviceProperties.fetchAndStoreIfChanged();
    simpleOTA->begin(PORTAL_SERVER_IP, deviceId.c_str(), true);
  }
}

void loop() {
  wifiProvision.loop();
  simpleOTA->checkUpdates(300);
  const char *propName = deviceProperties.Get("pub_topic");
  LOG("pub_topic=%s\n", propName);
  delay(5000);
}

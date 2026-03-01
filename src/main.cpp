#include "DeviceSetupManager.h"
#include "LiteWiFiManager.h"
#include "MyDeviceProperties.h"
#include "SimpleOTA.h"
#include <CommonDebug.h>
#include <Arduino.h>

#define LOG(fmt, ...) DBG_LOGF("*MAIN:", fmt, ##__VA_ARGS__)

SimpleOTA *simpleOTA = new SimpleOTA();
MyDeviceProperties deviceProperties;
LiteWiFiManager wifiProvision;
DeviceSetupManager setupMgr;

void setup() {
  Serial.begin(115200);
  wifiProvision.begin("ProjectSetup");

  if (!setupMgr.begin()) {
    LOG("DeviceSetupManager begin failed\n");
    return;
  }

  if (strlen(setupMgr.deviceId()) == 0) {
    LOG("Device ID not settled. please provide one.\n");
  }
  if (strlen(setupMgr.deviceSecret()) == 0) {
    LOG("Device secret not settled. please provide one.\n");
  }
  if (strlen(setupMgr.deviceTypeId()) == 0) {
    LOG("Device type ID not settled. please provide one.\n");
  }
  if (strlen(setupMgr.portalServerIp()) == 0) {
    LOG("Portal server IP not settled. please provide one.\n");
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
  }
}

void loop() {
  wifiProvision.loop();
  simpleOTA->checkUpdates(300);
  const char *value = deviceProperties.Get("key");
  LOG("value=%s\n", value);
  delay(5000);
}

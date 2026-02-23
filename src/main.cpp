#include "DeviceSetupManager.h"
#include "LiteWiFiManager.h"
#include "MyDeviceProperties.h"
#include "SimpleOTA.h"
#include "secret_data.h"
#include <Arduino.h>

SimpleOTA *simpleOTA = new SimpleOTA();
MyDeviceProperties deviceProperties;
LiteWiFiManager wifiProvision;
DeviceSetupManager setupMgr;
String deviceId;

void setup() {
  Serial.begin(115200);
  wifiProvision.begin("ProjectSetup");

  if (!setupMgr.begin()) {
    Serial.println("DeviceSetupManager begin failed");
  } else {
    deviceId = setupMgr.readDeviceId();
    if (deviceId.isEmpty()) {
      Serial.println("Device ID not settled. please provide one.");
    }
  }

  Serial.printf("DEVICE ID [%s]\n", deviceId.c_str());

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
  Serial.println(propName);
  delay(5000);
}

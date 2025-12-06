#include <Arduino.h>
#include "HardwareSerial.h"
#include "SimpleOTA.h"
#include "WiFiManager.h"
#include "secret_data.h"
#include "MyDeviceProperties.h"

SimpleOTA *simpleOTA = new SimpleOTA();
MyDeviceProperties deviceProperties;

void setup() {
  Serial.begin(115200);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect("Project Name");

  if (WiFi.status() == WL_CONNECTED) {
    deviceProperties.begin(PORTAL_SERVER_IP, DEVICE_TYPE_ID);
    deviceProperties.fetchAndStoreIfChanged();
    Serial.println("starting OTA");
    simpleOTA->begin(512, PORTAL_SERVER_IP, DEVICE_TYPE_ID, false);
  }
}

void loop() {
  simpleOTA->checkUpdates(300);
}

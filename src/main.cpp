#include <Arduino.h>
#include "HardwareSerial.h"
#include "SimpleOTA.h"
#include "WiFiManager.h"
#include "core_esp8266_features.h"
#include "secret_data.h"

SimpleOTA *simpleOTA = new SimpleOTA();

void setup() {
  Serial.begin(115200);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect("Project Name");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("starting OTA");
    simpleOTA->begin(512, PORTAL_SERVER_IP, DEVICE_TYPE_ID, false);
  }
}

void loop() {
  simpleOTA->checkUpdates(300);
}

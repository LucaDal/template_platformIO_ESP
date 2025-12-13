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
const char *DEVICE_ID;

void setup() {
  Serial.begin(115200);
  // Try stored credentials; fall back to minimal portal if needed.
  wifiProvision.begin("ProjectSetup");

  size_t nextOffset = 0;
    // Setup device previously settled up
  setupMgr.begin();
  DEVICE_ID = setupMgr.readCString(0, &nextOffset);
  Serial.printf("DEVICE ID [%s]\n", DEVICE_ID);

  if (WiFi.status() == WL_CONNECTED) {
  //  deviceProperties.begin(PORTAL_SERVER_IP, DEVICE_ID, nextOffset);
  //  deviceProperties.fetchAndStoreIfChanged();
    simpleOTA->begin(512, PORTAL_SERVER_IP, DEVICE_ID, true);
  }
}

void loop() {
  wifiProvision.loop();
  simpleOTA->checkUpdates(300);
  JsonDocument &doc = deviceProperties.json();
  const char *propName = doc["pub_topic"] | "";
  Serial.println(propName);
  delay(5000);
}

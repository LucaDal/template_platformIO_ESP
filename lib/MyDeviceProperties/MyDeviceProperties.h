#ifndef MY_DEVICE_PROPERTIES_H
#define MY_DEVICE_PROPERTIES_H

#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

//#define DEBUG

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <HTTPClient.h>
#include <WiFi.h>
#endif

#ifdef USE_TLS
  #ifdef ESP8266
    #include <WiFiClientSecureBearSSL.h>
    #include <BearSSLHelpers.h>
  #elif defined(ESP32)
    #include <WiFiClientSecure.h>
  #endif
  #include "Certs.h"
#endif

#ifdef USE_TLS
  #ifdef ESP8266
    using NetClient = BearSSL::WiFiClientSecure;
  #elif defined(ESP32)
    using NetClient = WiFiClientSecure;
  #endif
#else
  using NetClient = WiFiClient;
#endif

#ifdef DEBUG
  #define MYPROPS_LOG(msg) Serial.printf(PSTR("*MyProps: %s\n"), (msg))
  #define MYPROPS_LOGF(fmt, ...) Serial.printf(PSTR("*MyProps: " fmt), ##__VA_ARGS__)
#else
  #define MYPROPS_LOG(msg) ((void)0)
  #define MYPROPS_LOGF(...) ((void)0)
#endif

class MyDeviceProperties {
public:
  MyDeviceProperties(size_t eepromSize = 512, size_t eepromOffset = 0,
                     size_t reservedTailBytes = 3, size_t jsonCapacity = 512,
                     bool verifyCert = false);

  bool begin(const char *serverAddress, const char *deviceTypeId);
  bool fetchAndStoreIfChanged();
  bool loadFromEEPROM();
  JsonDocument &json();

private:
  String buildUrl() const;
  bool saveToEEPROM(const String &payload);
  String readPayloadFromEEPROM();
  size_t availableStorage() const;

  String serverAddress;
  String deviceTypeId;
  size_t eepromSize;
  size_t eepromOffset;
  size_t reservedTailBytes;
  bool verifyCert;
  JsonDocument doc;
  String cachedPayload;
#ifdef USE_TLS
  #ifdef ESP8266
    BearSSL::X509List trustedRoots;
  #endif
#endif
  std::unique_ptr<NetClient> client { new NetClient };
};

#endif

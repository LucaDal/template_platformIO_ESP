#include "MyDeviceProperties.h"

MyDeviceProperties::MyDeviceProperties(size_t eepromSize, size_t eepromOffset,
                                       size_t reservedTailBytes,
                                       size_t jsonCapacity, bool verifyCert)
    : eepromSize(eepromSize), eepromOffset(eepromOffset),
      reservedTailBytes(reservedTailBytes), verifyCert(verifyCert) {
  (void)jsonCapacity;
#ifdef USE_TLS
  #ifdef ESP8266
    #ifdef USE_TLS_CERTS
      trustedRoots.append(cert_ISRG_X1);
      trustedRoots.append(cert_ISRG_X2);
      if (verifyCert) {
        client->setTrustAnchors(&trustedRoots);
        client->setSSLVersion(BR_TLS12, BR_TLS12);
      } else {
        client->setInsecure();
      }
    #else
      client->setInsecure();
    #endif
  #elif defined(ESP32)
    #ifdef USE_TLS_CERTS
      if (verifyCert) {
        client->setCACert(cert_ISRG_X1);
      } else {
        client->setInsecure();
      }
    #else
      client->setInsecure();
    #endif
  #endif
#endif
}

bool MyDeviceProperties::begin(const char *serverAddress,
                               const char *deviceTypeId) {
  return begin(serverAddress, deviceTypeId, this->eepromOffset);
}

bool MyDeviceProperties::begin(const char *serverAddress,
                               const char *deviceTypeId,
                               size_t eepromOffset) {
  this->serverAddress = serverAddress;
  this->deviceTypeId = deviceTypeId;
  this->eepromOffset = eepromOffset;
  MYPROPS_LOGF("begin server=%s device=%s offset=%u\n", serverAddress,
               deviceTypeId, static_cast<unsigned>(eepromOffset));
  EEPROM.begin(eepromSize);
  return loadFromEEPROM();
}

JsonDocument &MyDeviceProperties::json() { return doc; }

String MyDeviceProperties::buildUrl() const {
  String protocol = "http://";
#ifdef USE_TLS
  protocol = "https://";
#endif
  return protocol + serverAddress + "/ota/" + deviceTypeId + "/properties";
}

size_t MyDeviceProperties::availableStorage() const {
  size_t reserved = eepromOffset + reservedTailBytes + sizeof(uint16_t);
  if (reserved >= eepromSize) {
    return 0;
  }
  return eepromSize - reserved;
}

String MyDeviceProperties::readPayloadFromEEPROM() {
  uint16_t len = EEPROM.read(eepromOffset) |
                 (EEPROM.read(eepromOffset + 1) << 8);
  if (len == 0 || len > availableStorage()) {
    MYPROPS_LOG("eeprom empty");
    return "";
  }
  String payload;
  payload.reserve(len);
  for (uint16_t i = 0; i < len; i++) {
    payload += static_cast<char>(EEPROM.read(eepromOffset + 2 + i));
  }
  return payload;
}

bool MyDeviceProperties::saveToEEPROM(const String &payload) {
  size_t space = availableStorage();
  if (payload.length() > space) {
    MYPROPS_LOG("not enough space on eeprom");
    return false;
  }
  uint16_t len = static_cast<uint16_t>(payload.length());
  EEPROM.write(eepromOffset, len & 0xFF);
  EEPROM.write(eepromOffset + 1, (len >> 8) & 0xFF);
  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(eepromOffset + 2 + i, static_cast<uint8_t>(payload[i]));
  }
  bool ok = EEPROM.commit();
  if (ok) {
    cachedPayload = payload;
  }
  return ok;
}

bool MyDeviceProperties::loadFromEEPROM() {
  String payload = readPayloadFromEEPROM();
  if (payload.isEmpty()) {
    doc.clear();
    MYPROPS_LOG("eeprom payload empty");
    return false;
  }
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    doc.clear();
    MYPROPS_LOG("eeprom parse fail");
    return false;
  }
  cachedPayload = payload;
  MYPROPS_LOG("eeprom payload loaded");
  return true;
}

bool MyDeviceProperties::fetchAndStoreIfChanged() {
  if (!WiFi.isConnected()) {
    MYPROPS_LOG("wifi down");
    return false;
  }

  String url = buildUrl();
  MYPROPS_LOGF("get %s\n", url.c_str());
  HTTPClient http;
#ifdef USE_TLS
  // no-op when not verifying, set in constructor
  if (verifyCert) {
    // On ESP8266 buffer reduction helps with memory usage when using TLS
  #ifdef ESP8266
    client->setBufferSizes(512, 264);
  #endif
  }
#endif

  bool httpConnected = http.begin(*client, url);
  if (!httpConnected) {
    http.end();
    MYPROPS_LOG("http begin fail");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    MYPROPS_LOGF("http code %d\n", httpCode);
    return false;
  }

  String payload = http.getString();
  http.end();

  MYPROPS_LOGF("payload %s\n", payload.c_str());

  if (payload == cachedPayload) {
    MYPROPS_LOG("unchanged");
    return true;
  }

  if (payload.length() > availableStorage()) {
    MYPROPS_LOG("payload too big");
    return false;
  }

  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    MYPROPS_LOG("json parse fail");
    return false;
  }

  return saveToEEPROM(payload);
}

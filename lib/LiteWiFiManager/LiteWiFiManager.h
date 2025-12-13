#pragma once

#include <Arduino.h>
#include <CommonDebug.h>

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  using LiteWiFiWebServer = ESP8266WebServer;
#else
  #include <WiFi.h>
  #include <WebServer.h>
  using LiteWiFiWebServer = WebServer;
#endif
#include <DNSServer.h>

class LiteWiFiManager {
 public:
  explicit LiteWiFiManager(Print *logger = &Serial);

  // Attempts to connect using credentials already stored in the WiFi core.
  // If that fails (or forcePortal is true), starts a tiny AP + form to collect
  // new credentials. Returns true when connected to WiFi.
  bool begin(const char *apSsid,
             const char *apPassword = nullptr,
             unsigned long configPortalTimeoutMs = 180000,
             bool forcePortal = false);

  // Pump this in loop() to serve the portal while active.
  void loop();

  bool isPortalActive() const { return _portalActive; }
  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

 private:
  bool connectWithStored(unsigned long connectTimeoutMs = 10000);
  bool connectWithNew(const String &ssid,
                      const String &password,
                      unsigned long connectTimeoutMs = 10000);

  void startPortal(const char *apSsid,
                   const char *apPassword,
                   unsigned long timeoutMs);
  void stopPortal();

  void handleRoot();
  void handleSave();
  void handleScan();

  LiteWiFiWebServer _server;
  DNSServer _dns;
  bool _portalActive = false;
  bool _connectRequested = false;
  unsigned long _portalDeadline = 0;
  String _newSsid;
  String _newPass;
};

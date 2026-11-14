#include "DeviceSetupManager.h"
#include "FirmwareData.h"
#include "LiteWiFiManager.h"
#include "SimpleOTA.h"
#include <ArduinoJson.h>
#include <fstream>

void provision() {
  deviceMock::files["/provisioning/device.json"] = R"({"version":1,"deviceCode":"device-01","deviceSecret":"provisioning-secret","deviceTypeId":"sensor","portalServer":"portal.test/api","wifi_ssid":"Host Setup"})";
  deviceMock::files["/device/fw_version.txt"] = "1.2.3";
}

int main(int argc, char **argv) {
  assert(argc == 2 || argc == 3);
  std::string scenario = argv[1];
  provision();
  if (scenario == "provisioning-contract") {
    assert(argc == 3);
    std::ifstream input(argv[2]);
    assert(input.good());
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    JsonDocument fixtures;
    assert(!deserializeJson(fixtures, source));
    for (JsonObjectConst fixture : fixtures.as<JsonArrayConst>()) {
      const std::string data = fixture["source"].as<std::string>();
      deviceMock::files["/provisioning/device.json"] = data;
      DeviceSetupManager setup;
      const bool expected = fixture["valid"].as<bool>();
      assert(setup.begin() == expected && setup.isProvisioningReady() == expected);
      if (expected) {
        JsonDocument config;
        assert(!deserializeJson(config, data));
        assert(std::string(setup.deviceCode()) == config["deviceCode"].as<std::string>());
        assert(std::string(setup.wifiSsid()) == config["wifi_ssid"].as<std::string>());
      } else {
        assert(!setup.deviceSecret()[0]);
      }
    }
    return 0;
  }
  if (scenario == "missing-config") {
    deviceMock::files.erase("/provisioning/device.json");
    deviceMock::files["/provisioning/device_code.txt"] = "legacy-device";
    deviceMock::files["/provisioning/device_secret.txt"] = "legacy-secret";
    DeviceSetupManager setup;
    assert(!setup.begin() && !setup.isProvisioningReady());
    assert(setup.wifiApPassword().isEmpty() && !setup.wifiSsid()[0]);
    return 0;
  }
  if (scenario == "invalid-config") {
    const std::string original = deviceMock::files.at("/provisioning/device.json");
    auto reject = [](const JsonDocument &document) {
      std::string source;
      serializeJson(document, source);
      deviceMock::files["/provisioning/device.json"] = source;
      DeviceSetupManager setup;
      assert(!setup.begin() && !setup.isProvisioningReady());
      assert(setup.wifiApPassword().isEmpty() && !setup.deviceCode()[0]);
    };
    for (const char *key : {"portalServer", "deviceCode", "deviceTypeId", "deviceSecret", "wifi_ssid", "version"}) {
      JsonDocument document;
      assert(!deserializeJson(document, original));
      document.remove(key);
      reject(document);
    }
    for (const char *key : {"portalServer", "deviceCode", "deviceTypeId", "deviceSecret", "wifi_ssid"}) {
      for (const char *value : {"", "bad\nvalue"}) {
        JsonDocument document;
        assert(!deserializeJson(document, original));
        document[key] = value;
        reject(document);
      }
    }
    for (const char *bad : {"version", "deviceSecret", "wifi_ssid"}) {
      JsonDocument document;
      assert(!deserializeJson(document, original));
      document[bad] = true;
      reject(document);
    }
    JsonDocument document;
    assert(!deserializeJson(document, original));
    document["wifi_ssid"] = std::string(33, 'a');
    reject(document);
    for (const char *source : {"{", "[]", "null"}) {
      deviceMock::files["/provisioning/device.json"] = source;
      DeviceSetupManager setup;
      assert(!setup.begin());
    }
    deviceMock::files["/provisioning/device.json"] = std::string(4097, ' ');
    DeviceSetupManager oversized;
    assert(!oversized.begin());
    return 0;
  }
  if (scenario == "provision-write-failure" || scenario == "provision-rename-failure") {
    DeviceSetupManager setup;
    assert(setup.begin());
    const auto original = deviceMock::files.at("/provisioning/device.json");
    deviceMock::failWrite = scenario == "provision-write-failure";
    deviceMock::failRename = scenario == "provision-rename-failure";
    assert(!setup.saveDeviceSecret("replacement-secret"));
    assert(deviceMock::files.at("/provisioning/device.json") == original);
    assert(std::string(setup.deviceSecret()) == "provisioning-secret");
    assert(!deviceMock::files.count("/provisioning/device.json.tmp"));
    return 0;
  }
  if (scenario == "mount-failure") {
    auto files = deviceMock::files;
    deviceMock::fsMounted = false;
    DeviceSetupManager setup;
    assert(!setup.begin());
    FirmwareData firmware;
    assert(!firmware.saveVersion("2.0.0"));
    assert(!deviceMock::formatRequested && deviceMock::files == files);
    return 0;
  }
  if (scenario == "ap-password") {
    DeviceSetupManager setup;
    assert(setup.wifiApPassword().isEmpty());
    assert(setup.begin() && setup.isProvisioningReady());
    assert(std::string(setup.wifiSsid()) == "Host Setup");
    assert(setup.wifiApPassword() == "7b1441c36db3f42d901dbb77c5437ede");
    assert(setup.saveDeviceSecret("other-secret"));
    assert(setup.wifiApPassword() == "4d1a181c0df784493610ae48003fd5e0");
    assert(setup.saveDeviceCode("device-02"));
    JsonDocument saved;
    assert(!deserializeJson(saved, deviceMock::files.at("/provisioning/device.json")));
    assert(saved["deviceCode"] == "device-02" && saved["deviceSecret"] == "other-secret");
    assert(saved["wifi_ssid"] == "Host Setup" && saved["portalServer"] == "portal.test/api");
    const auto original = deviceMock::files.at("/provisioning/device.json");
    assert(!setup.saveDeviceSecret("bad\r\nsecret") && !setup.saveDeviceCode(""));
    assert(!setup.saveDeviceSecret(std::string(4096, 'a').c_str()));
    assert(deviceMock::files.at("/provisioning/device.json") == original);
    assert(std::string(setup.deviceSecret()) == "other-secret");
    assert(!deviceMock::formatRequested);
    return 0;
  }
  if (scenario == "ota-recovery") {
    deviceMock::connected = true;
    SimpleOTA ota;
    ota.begin("portal.test/api", "sensor", "device-01", "secret", true);
    assert(ota.checkUpdates(0) && deviceMock::downloads == 0);
    deviceMock::serverVersion = "-1";
    assert(!ota.checkUpdates(0) && deviceMock::downloads == 0);
    deviceMock::serverVersion = "1.2.3";
    assert(ota.checkUpdates(0) && deviceMock::downloads == 0);
    deviceMock::serverVersion = "1.2.4";
    assert(!ota.checkUpdates(0) && deviceMock::downloads == 1);
    assert(deviceMock::downloadCurrentVersion == "1.2.3");
    assert(!ota.checkUpdates(0) && deviceMock::downloads == 2);
    assert(deviceMock::downloadCurrentVersion == "1.2.3");
    deviceMock::serverVersion = "1.2.3";
    assert(ota.checkUpdates(0) && deviceMock::downloads == 2);
    assert(deviceMock::files.at("/device/fw_version.txt") == "1.2.3");
    return 0;
  }
  if (scenario == "version-save" || scenario == "version-write-failure" || scenario == "version-rename-failure") {
    FirmwareData firmware;
    firmware.setNewFirmware({"1.2.4", "checksum"});
    assert(firmware.hasNewUpdate() && firmware.getOldFirmwareVersion() == "1.2.3");
    deviceMock::failWrite = scenario == "version-write-failure";
    deviceMock::failRename = scenario == "version-rename-failure";
    const bool success = scenario == "version-save";
    assert(firmware.saveVersion("1.2.4") == success);
    assert(deviceMock::files.at("/device/fw_version.txt") == (success ? "1.2.4" : "1.2.3"));
    assert(firmware.getOldFirmwareVersion() == (success ? "1.2.4" : "1.2.3"));
    assert(firmware.hasNewUpdate() != success);
    assert(!deviceMock::files.count("/device/fw_version.txt.tmp"));
    return 0;
  }
  if (scenario == "open-ap" || scenario == "short-ap-password" || scenario == "ap-start-failure") {
    LiteWiFiManager wifi;
    deviceMock::apStarts = scenario != "ap-start-failure";
    const char *password = scenario == "open-ap" ? nullptr : scenario == "short-ap-password" ? "short" : "test-ap-password";
    assert(!wifi.begin("Setup", password, 100));
    assert(!wifi.isPortalActive());
    assert(deviceMock::apCalls == (scenario == "ap-start-failure" ? 1 : 0));
    return 0;
  }
  if (scenario == "stored-wifi") {
    LiteWiFiManager wifi;
    deviceMock::storedConnects = true;
    assert(wifi.begin("Setup", "test-ap-password"));
    assert(deviceMock::apCalls == 0);
    return 0;
  }
  if (scenario == "portal") {
    LiteWiFiManager wifi;
    const std::string ssid = "Cafe\"Guest\\\n";
    deviceMock::networks = {ssid};
    deviceMock::requests.push_back({HTTP_GET, "/scan", {}});
    deviceMock::requests.push_back({HTTP_GET, "/save", {{"ssid", "attacker"}, {"pass", "wrong-password"}}});
    deviceMock::requests.push_back({HTTP_POST, "/save", {{"ssid", " Cafe WiFi "}, {"pass", " password with spaces "}}});
    assert(wifi.begin("Setup", "test-ap-password", 1000));
    assert(deviceMock::apPassword == "test-ap-password");
    assert(deviceMock::savedSsid == " Cafe WiFi ");
    assert(deviceMock::savedPassword == " password with spaces ");
    assert(deviceMock::responses.size() == 3);
    assert(deviceMock::responses[0].status == 200);
    assert(deviceMock::responses[1].status == 302);
    assert(deviceMock::responses[2].status == 200);
    JsonDocument scan;
    assert(!deserializeJson(scan, deviceMock::responses[0].body));
    assert(scan["networks"][0]["ssid"].as<std::string>() == ssid);
    assert(!wifi.isPortalActive() && deviceMock::wifiMode == WIFI_STA);
    return 0;
  }
  assert(false);
}

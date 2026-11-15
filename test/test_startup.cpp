#include "../src/main.cpp"

bool messageContains(const std::string &text) {
  return std::any_of(mock::messages.begin(), mock::messages.end(), [text](const auto &value) {
    return value.find(text) != std::string::npos;
  });
}

int main(int argc, char **argv) {
  assert(argc == 2);
  std::string scenario = argv[1];
  const bool combinedRetry = scenario == "portal-retry-and-ota" || scenario == "ota-due-during-fetch";
  if (scenario == "storage-failure") mock::storageReady = false;
  if (scenario == "missing-provisioning") mock::provisioningReady = false;
  if (scenario == "mqtt-init-failure") mock::mqttReady = false;
  if (scenario == "schedule-unavailable") mock::scheduleReady = false;
  if (scenario == "startup-in-window") mock::otaDue = true;
  if (scenario == "wifi-retry" || scenario == "wifi-core-first") {
    mock::wifiShouldConnect = false;
    mock::wifiDuration = 100000;
  }
  if (scenario == "cache-fallback" || scenario == "no-cache-retry" || combinedRetry) mock::fetchSuccess = false;
  if (scenario == "no-cache-retry" || scenario == "first-install") mock::cached = false;
  setup();
  assert(mock::wifiAlive == 0);
  assert(mock::fetchCalls == 0 && mock::otaCalls == 0 && mock::mqttCalls == 0);
  assert(std::find(mock::events.begin(), mock::events.end(), "provisioning") != mock::events.end());

  if (scenario == "storage-failure" || scenario == "missing-provisioning" || scenario == "mqtt-init-failure") {
    assert(!initialized && mock::wifiCalls == 0);
    loop();
    assert(mock::fetchCalls == 0 && mock::otaCalls == 0 && mock::mqttCalls == 0);
    return 0;
  }
  assert(initialized);
  if (scenario == "wifi-initial-drop") {
    mock::wifiConnected = false;
    mock::wifiShouldConnect = false;
    mock::now = lastWiFiAttempt + RETRY_INTERVAL_MS * 2;
    loop();
    assert(mock::wifiCalls == 1 && mock::fetchCalls == 0 && mock::otaCalls == 0);
    mock::wifiConnected = true;
  }
  if (scenario == "wifi-core-first") {
    mock::wifiConnected = true;
    loop();
    mock::wifiConnected = false;
    mock::now = lastWiFiAttempt + RETRY_INTERVAL_MS * 2;
    loop();
    assert(mock::wifiCalls == 1);
    return 0;
  }
  if (scenario == "wifi-retry") {
    assert(mock::wifiCalls == 1);
#ifdef DEBUG
    assert(messageContains("retrying in " + std::to_string(RETRY_INTERVAL_MS / 1000UL) + " seconds."));
#endif
    mock::now = lastWiFiAttempt + RETRY_INTERVAL_MS - 1;
    loop();
    assert(mock::wifiCalls == 1 && mock::fetchCalls == 0 && mock::mqttCalls == 0);
    mock::now = lastWiFiAttempt + RETRY_INTERVAL_MS;
    mock::wifiShouldConnect = true;
    loop();
    assert(mock::wifiCalls == 2 && mock::wifiConnected && mock::wifiAlive == 0);
  }
  auto disconnects = std::count(mock::events.begin(), mock::events.end(), "mqtt.disconnect");
  loop();
  assert(mock::fetchCalls == 1 && mock::otaCalls == 1 && mock::mqttCalls == 1);
  assert(std::count(mock::events.begin(), mock::events.end(), "mqtt.disconnect") == disconnects + 1);
  auto fetched = std::find(mock::events.begin(), mock::events.end(), "properties.fetch");
  auto checked = std::find(mock::events.begin(), mock::events.end(), "ota.check:0");
  auto connected = std::find(mock::events.begin(), mock::events.end(), "mqtt.connect");
  assert(fetched < checked && checked < connected);
  assert(!initialOtaCheckPending);

  if (scenario == "schedule-unavailable" || scenario == "startup-in-window") {
    mock::otaDue = true;
    loop();
    assert(mock::otaCalls == 1 && mock::mqttCalls == 1);
    return 0;
  }
  if (scenario == "offline-window") {
    mock::wifiConnected = false;
    mock::otaDue = true;
    loop();
    assert(mock::otaCalls == 1);
    mock::otaDue = false;
    mock::wifiConnected = true;
    loop();
    assert(mock::otaCalls == 1 && mock::mqttCalls == 2);
    mock::otaDue = true;
    loop();
    assert(mock::otaCalls == 2 && mock::mqttCalls == 3);
    return 0;
  }

  if (scenario == "cache-fallback" || scenario == "no-cache-retry" || combinedRetry) {
    assert(!propertiesFetched);
    assert(deviceProperties.json().available == mock::cached);
#ifdef DEBUG
    assert(messageContains(mock::cached ? "using cached values" : "Properties unavailable"));
    assert(messageContains(std::to_string(RETRY_INTERVAL_MS / 1000UL) + " seconds."));
#endif
    mock::now = lastPropertiesAttempt + RETRY_INTERVAL_MS - 1;
    loop();
    assert(mock::fetchCalls == 1);
    mock::now = lastPropertiesAttempt + RETRY_INTERVAL_MS;
    mock::fetchSuccess = true;
    mock::otaDue = scenario == "portal-retry-and-ota";
    mock::otaBecomesDueOnFetch = scenario == "ota-due-during-fetch";
    disconnects = std::count(mock::events.begin(), mock::events.end(), "mqtt.disconnect");
    auto retryStart = mock::events.size();
    loop();
    assert(mock::fetchCalls == 2 && propertiesFetched && deviceProperties.json().available);
    assert(std::count(mock::events.begin(), mock::events.end(), "mqtt.disconnect") == disconnects + 1);
    assert(mock::mqttCalls == 2 && mock::otaCalls == (combinedRetry ? 2 : 1));
    if (combinedRetry) {
      auto fetch = std::find(mock::events.begin() + retryStart, mock::events.end(), "properties.fetch");
      auto ota = std::find(fetch, mock::events.end(), "ota.check:0");
      auto mqtt = std::find(ota, mock::events.end(), "mqtt.connect");
      assert(fetch < ota && ota < mqtt && mqtt != mock::events.end());
    }
    return 0;
  }
  assert(propertiesFetched);
  loop();
  assert(mock::fetchCalls == 1 && mock::otaCalls == 1 && mock::mqttCalls == 1);
  mock::otaDue = true;
  loop();
  assert(mock::otaCalls == 2 && mock::mqttCalls == 2);
  assert(std::count(mock::events.begin(), mock::events.end(), "ota.check:0") == 2);
  mock::otaDue = true;
  loop();
  assert(mock::otaCalls == 2 && mock::mqttCalls == 2);
  int wifiAttempts = mock::wifiCalls;
  int mqttConnections = mock::mqttCalls;
  mock::wifiConnected = false;
  mock::wifiShouldConnect = false;
  mock::now = lastWiFiAttempt + RETRY_INTERVAL_MS * 2;
  loop();
  mock::now += RETRY_INTERVAL_MS * 2;
  loop();
  assert(mock::wifiCalls == wifiAttempts && !mock::mqttConnected);
  mock::wifiConnected = true;
  loop();
  assert(mock::wifiCalls == wifiAttempts && mock::mqttCalls == mqttConnections + 1);
  assert(!messageContains("secret"));
}

#include "SimpleOTA.h"
#include "ota_dependencies.h"

int main() {
  SimpleOTA ota;
  assert(!ota.isCheckDue(0) && !ota.checkUpdates(0));
  mock::wifiConnected = true;
  ota.begin("portal.test/api", "sensor", "device-code", "secret", true);
  assert(mock::otaCalls == 0);
  assert(ota.checkUpdates(0) && mock::otaCalls == 1);
  mock::now = 299999;
  assert(!ota.isCheckDue(300));
  assert(ota.checkUpdates(300) && mock::otaCalls == 1);
  mock::now = 300000;
  assert(ota.isCheckDue(300));
  assert(ota.checkUpdates(300) && mock::otaCalls == 2);
}

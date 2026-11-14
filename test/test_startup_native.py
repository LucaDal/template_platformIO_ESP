import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class StartupNativeTest(unittest.TestCase):
    def test_startup_and_explicit_ota_check(self):
        root = Path(__file__).resolve().parents[1]
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("Requires a C++ compiler")
        host = root / "test/startup_host"
        ota = root / "my-esp-libraries/EspOTA"
        daily = root / "my-esp-libraries/DailySchedule"
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            for header in ("Arduino.h", "DeviceSetupManager.h", "DailySchedule.h",
                           "LiteWiFiManager.h", "MQTTManager.h", "MyDeviceProperties.h", "SimpleOTA.h"):
                (temporary / header).write_text('#include "stubs.h"\n')
            debug_header = root / "my-esp-libraries/CommonDebug/CommonDebug.h"
            (temporary / "CommonDebug.h").write_text(f'#include "{debug_header}"\n')
            startup = temporary / "startup"
            for debug_flags in ([], ["-DDEBUG"]):
                subprocess.run([compiler, "-std=c++17", *debug_flags, "-I" + str(temporary), "-I" + str(host),
                                str(root / "test/test_startup.cpp"), "-o", str(startup)], check=True)
                for scenario in ("normal", "storage-failure", "missing-provisioning", "mqtt-init-failure",
                                 "wifi-retry", "cache-fallback", "no-cache-retry", "first-install",
                                 "wifi-initial-drop", "wifi-core-first", "portal-retry-and-ota",
                                 "ota-due-during-fetch", "schedule-unavailable", "startup-in-window",
                                 "offline-window"):
                    subprocess.run([str(startup), scenario], check=True)
            (temporary / "SimpleOTA.h").unlink()
            binary = temporary / "ota"
            subprocess.run([compiler, "-std=c++17", "-DTEST_REAL_OTA", "-DNETWORK_H", "-DFIRMWAREDATA_H",
                            "-I" + str(temporary), "-I" + str(host), "-I" + str(ota),
                            "-include", str(host / "ota_dependencies.h"),
                            str(root / "test/test_simple_ota.cpp"), str(ota / "SimpleOTA.cpp"),
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            schedule = temporary / "schedule"
            subprocess.run([compiler, "-std=c++17", "-I" + str(daily),
                            str(root / "test/test_daily_schedule.cpp"), str(daily / "DailySchedule.cpp"),
                            "-o", str(schedule)], check=True)
            subprocess.run([str(schedule)], check=True)

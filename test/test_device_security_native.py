import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class DeviceSecurityTest(unittest.TestCase):

    def test_real_device_libraries(self):
        root = Path(__file__).resolve().parents[1]
        compiler = shutil.which("c++")
        json = root / ".pio/libdeps/esp8266/ArduinoJson/src"
        if not compiler or not json.is_dir() or not Path("/usr/include/openssl/sha.h").is_file():
            self.skipTest("Requires c++, PlatformIO ArduinoJson, and OpenSSL development headers")
        libraries = root / "my-esp-libraries"
        with tempfile.TemporaryDirectory() as directory:
            for platform in ("ESP32", "ESP8266"):
                executable = Path(directory) / platform
                command = [compiler, "-std=c++17", "-Wno-deprecated-declarations", "-D" + platform,
                           "-I" + str(root / "test/device_host"), "-I" + str(json),
                           "-include", str(root / "test/device_host/network.h")]
                for name in ("CommonStorage", "EspOTA", "LiteWiFiManager", "DeviceSetupManager"):
                    command.append("-I" + str(libraries / name))
                sources = [root / "test/test_device_security.cpp", libraries / "EspOTA/FirmwareData.cpp",
                           libraries / "EspOTA/SimpleOTA.cpp", libraries / "DeviceSetupManager/DeviceSetupManager.cpp",
                           libraries / "LiteWiFiManager/LiteWiFiManager.cpp"]
                subprocess.run(command + [str(path) for path in sources] + ["-lcrypto", "-o", str(executable)], check=True)
                subprocess.run([str(executable), "provisioning-contract",
                                str(root / "test/fixtures/provisioning.json")], check=True)
                for scenario in ("missing-config", "invalid-config", "provision-write-failure",
                                 "provision-rename-failure", "mount-failure", "ap-password", "ota-recovery", "version-save",
                                 "version-write-failure", "version-rename-failure", "open-ap",
                                 "short-ap-password", "ap-start-failure", "stored-wifi", "portal"):
                    with self.subTest(platform=platform, scenario=scenario):
                        subprocess.run([str(executable), scenario], check=True)

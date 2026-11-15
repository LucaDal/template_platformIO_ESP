import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class MqttNativeTest(unittest.TestCase):
    def test_configuration_and_automatic_subscriptions(self):
        root = Path(__file__).resolve().parents[1]
        compiler = shutil.which("c++")
        arduino_json = root / ".pio/libdeps/esp8266/ArduinoJson/src"
        if not compiler or not arduino_json.is_dir():
            self.skipTest("Requires c++ and installed PlatformIO ArduinoJson dependencies")
        manager = root / "my-esp-libraries/MQTTManager"
        with tempfile.TemporaryDirectory() as directory:
            for name, platform in (("configuration", "ESP32"), ("manager", "ESP32"),
                                   ("features", "ESP32"), ("features", "ESP8266")):
                binary = Path(directory) / name
                command = [compiler, "-std=c++17", "-DUSE_TLS", "-D" + platform,
                           "-I" + str(root / "test/mqtt_host"),
                           "-I" + str(arduino_json), "-I" + str(manager),
                           str(root / ("test/test_mqtt_" + name + ".cpp"))]
                if name != "configuration":
                    command.append(str(manager / "MQTTManager.cpp"))
                subprocess.run(command + ["-o", str(binary)], check=True)
                subprocess.run([str(binary)], check=True)

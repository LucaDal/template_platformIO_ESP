import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

from scripts.ota_size import write_flash_report


class OtaSizeTest(unittest.TestCase):
    def test_report_uses_platformio_size_command_pattern_and_board_limit(self):
        class Environment(dict):
            def subst(self, value):
                if isinstance(value, list):
                    return value
                return value.replace("$BUILD_DIR", str(build)).replace("${PROGNAME}", "firmware")

            def BoardConfig(self):
                return {"upload.maximum_size": 958448}

        with tempfile.TemporaryDirectory(prefix="OTA size ") as directory:
            build = Path(directory)
            firmware = build / "firmware.bin"
            firmware.write_bytes(b"firmware")
            environment = Environment({
                "ENV": {"PATH": os.environ["PATH"]},
                "SIZECHECKCMD": [sys.executable, "-c",
                                 "import sys; print('.text 500000'); print('.data 11913'); "
                                 "print('.bss 50000'); assert sys.argv[1].endswith('firmware.elf')",
                                 "$SOURCES"],
                "SIZEPROGREGEXP": r"^(?:\.text|\.data)\s+(\d+)$",
            })
            write_flash_report(environment)
            report = json.loads(Path(str(firmware) + ".ota-size.json").read_text())
            self.assertEqual(report["flash_used"], 511913)
            self.assertEqual(report["flash_total"], 958448)
            self.assertEqual(report["firmware_sha256"], hashlib.sha256(b"firmware").hexdigest())
            environment["SIZEPROGREGEXP"] = "^missing-pattern$"
            with self.assertRaisesRegex(ValueError, "Cannot determine PlatformIO Flash usage"):
                write_flash_report(environment)

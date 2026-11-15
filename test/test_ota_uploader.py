import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from requests import HTTPError, Timeout

from scripts import upload_command_ota_script as uploader


class OtaUploaderTest(unittest.TestCase):
    def test_all_values_come_from_project_dotenv_without_provisioning_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / ".env").write_text(
                "OTA_SERVER_ADDRESS=portal.test/api\nOTA_DEVICE_TYPE=sensor\n"
                "OTA_EMAIL=admin@example.org\nOTA_PASSWORD=' password with spaces '\n")
            with patch.object(uploader, "__file__", str(root / "scripts/upload_command_ota_script.py")), \
                 patch.dict(os.environ, {}, clear=True):
                self.assertEqual(uploader.get_ota_config(),
                                 ("portal.test/api", "sensor", "admin@example.org", " password with spaces "))

    def test_environment_overrides_all_dotenv_values(self):
        values = {"OTA_SERVER_ADDRESS": "portal.test/api", "OTA_DEVICE_TYPE": "sensor",
                  "OTA_EMAIL": "admin", "OTA_PASSWORD": "password"}
        environment = {"OTA_SERVER_ADDRESS": "other.test/api", "OTA_DEVICE_TYPE": "relay",
                       "OTA_EMAIL": "other-admin", "OTA_PASSWORD": "other-password"}
        with patch.object(uploader, "load_dotenv", return_value=values), \
             patch.dict(os.environ, environment, clear=True):
            self.assertEqual(uploader.get_ota_config(),
                             ("other.test/api", "relay", "other-admin", "other-password"))

    def test_missing_or_empty_configuration_stops_before_upload(self):
        values = {"OTA_SERVER_ADDRESS": "portal.test/api", "OTA_DEVICE_TYPE": "sensor",
                  "OTA_EMAIL": "admin", "OTA_PASSWORD": "password"}
        for key in values:
            for missing in (True, False):
                config = dict(values)
                if missing:
                    config.pop(key)
                else:
                    config[key] = ""
                with self.subTest(key=key, missing=missing), \
                     patch.object(uploader, "load_dotenv", return_value=config), \
                     patch.dict(os.environ, {}, clear=True), \
                     patch("sys.argv", ["uploader", "firmware.bin"]), \
                     patch("sys.stderr", new_callable=io.StringIO) as error, \
                     patch.object(uploader, "start_upload") as upload:
                    self.assertEqual(uploader.main(), 1)
                    self.assertIn(key, error.getvalue())
                    self.assertNotIn("password", error.getvalue())
                    upload.assert_not_called()

    def test_version_uses_admin_headers_and_new_route(self):
        with patch.object(uploader, "get") as request:
            request.return_value.text = '{"version":"1.2.3","md5Checksum":"checksum"}'
            with patch("sys.stdout", new_callable=io.StringIO):
                version = uploader.get_version_from_token("portal.test/api", "sensor", "admin", "password")
        self.assertEqual(version, "1.2.3")
        request.assert_called_once_with(
            "https://portal.test/api/ota/firmware/version",
            headers={"Authorization": "Basic YWRtaW46cGFzc3dvcmQ=", "x-device-type-id": "sensor"},
            timeout=30,
        )

    def test_upload_uses_put_and_preserves_multipart_fields(self):
        for bump, expected_version in [("patch", "1.2.4"), ("same", "1.2.3")]:
            with self.subTest(bump=bump), \
                 patch.object(uploader, "get_version_from_token", return_value="1.2.3"), \
                 patch.object(uploader, "ask_version_bump", return_value=bump), \
                 patch.object(uploader, "put") as request, \
                 patch.object(uploader.Path, "read_bytes", return_value=b"firmware"), \
                 patch("sys.stdout", new_callable=io.StringIO):
                request.return_value.text = '{"ok":"updated"}'
                self.assertTrue(uploader.start_upload("firmware.bin", "sensor", "portal.test/api", "admin", "password"))
                request.assert_called_once()
                kwargs = request.call_args.kwargs
                self.assertEqual(kwargs["url"], "https://portal.test/api/ota/firmware")
                self.assertEqual(kwargs["headers"], uploader.build_headers("sensor", "admin", "password"))
                self.assertEqual(kwargs["data"], {"version": expected_version})
                self.assertEqual(kwargs["files"]["file"], ("firmware.bin", b"firmware", "application/octet-stream"))
                self.assertEqual(kwargs["timeout"], 60)

    def test_esp8266_upload_compresses_the_firmware(self):
        with patch.object(uploader, "get_version_from_token", return_value="1.2.3"), \
             patch.object(uploader, "ask_version_bump", return_value="patch"), \
             patch.object(uploader, "put") as request, \
             patch.object(uploader.Path, "read_bytes", return_value=b"firmware"), \
             patch("sys.stdout", new_callable=io.StringIO):
            request.return_value.text = '{"ok":"updated"}'
            uploader.start_upload("firmware.bin", "sensor", "portal.test/api", "admin", "password", compress=True)
        filename, content, content_type = request.call_args.kwargs["files"]["file"]
        self.assertEqual(filename, "firmware.bin.gz")
        self.assertEqual(content_type, "application/octet-stream")
        self.assertEqual(gzip.decompress(content), b"firmware")
        self.assertEqual(content, gzip.compress(b"firmware", mtime=0))

    def test_esp8266_reports_gzip_size_and_platformio_headroom(self):
        original = b"firmware"
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(original)
            Path(str(firmware) + ".ota-size.json").write_text(json.dumps({
                "flash_used": 511913, "flash_total": 958448,
                "firmware_sha256": hashlib.sha256(original).hexdigest(),
            }))
            with patch.object(uploader, "get_version_from_token", return_value="1.2.3"), \
                 patch.object(uploader, "ask_version_bump", return_value="patch"), \
                 patch.object(uploader, "put") as request, \
                 patch("sys.stdout", new_callable=io.StringIO) as output:
                request.return_value.text = '{"ok":"updated"}'
                self.assertTrue(uploader.start_upload(firmware, "sensor", "portal.test/api", "admin", "password",
                                                     compress=True))
            report = output.getvalue()
            compressed_size = len(request.call_args.kwargs["files"]["file"][1])
            self.assertIn(f"Firmware GZIP: {compressed_size} bytes", report)
            self.assertIn("PlatformIO Flash: used 511913 bytes from 958448 bytes", report)
            self.assertIn("OTA budget (estimated): 446535 bytes", report)
            self.assertIn(f"OTA size check: OK (estimated margin: {446535 - compressed_size} bytes)", report)
            self.assertIn("installed firmware and flash layout match this build", report)

    def test_oversized_gzip_is_not_published(self):
        original = b"firmware"
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(original)
            Path(str(firmware) + ".ota-size.json").write_text(json.dumps({
                "flash_used": 511913, "flash_total": 511920,
                "firmware_sha256": hashlib.sha256(original).hexdigest(),
            }))
            with patch.object(uploader, "get_version_from_token", return_value="1.2.3"), \
                 patch.object(uploader, "ask_version_bump", return_value="patch"), \
                 patch.object(uploader, "put") as request, \
                 patch("sys.stdout", new_callable=io.StringIO) as output:
                self.assertFalse(uploader.start_upload(firmware, "sensor", "portal.test/api", "admin", "password",
                                                      compress=True))
                request.assert_not_called()
                self.assertIn("OTA size check: INSUFFICIENT", output.getvalue())
                self.assertIn("Upload cancelled", output.getvalue())

    def test_missing_or_stale_flash_values_never_report_ok(self):
        original = b"firmware"
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(original)
            for report in (None, "invalid json", "[]", json.dumps({
                    "flash_used": 511913, "flash_total": 958448, "firmware_sha256": "old-firmware"})):
                metadata = Path(str(firmware) + ".ota-size.json")
                if report is not None:
                    metadata.write_text(report)
                with self.subTest(report=report), \
                     patch.object(uploader, "get_version_from_token", return_value="1.2.3"), \
                     patch.object(uploader, "ask_version_bump", return_value="patch"), \
                     patch.object(uploader, "put") as request, \
                     patch("sys.stdout", new_callable=io.StringIO) as output:
                    request.return_value.text = '{"ok":"updated"}'
                    self.assertTrue(uploader.start_upload(firmware, "sensor", "portal.test/api", "admin", "password",
                                                         compress=True))
                    request.assert_called_once()
                    self.assertIn("OTA size check: UNAVAILABLE", output.getvalue())
                    self.assertNotIn("OTA size check: OK", output.getvalue())

    def test_failed_upload_is_reported_to_platformio(self):
        with patch.object(uploader, "get_ota_config", return_value=("portal.test/api", "sensor", "admin", "password")), \
             patch.object(uploader, "get_version_from_token", return_value="1.2.3"), \
             patch.object(uploader, "ask_version_bump", return_value="patch"), \
             patch.object(uploader.Path, "read_bytes", return_value=b"firmware"), \
             patch.object(uploader, "put") as request, \
             patch("sys.argv", ["uploader", "firmware.bin"]), \
             patch("sys.stdout", new_callable=io.StringIO):
            request.return_value.ok = False
            request.return_value.status_code = 403
            request.return_value.text = '{"error":"Forbidden"}'
            self.assertEqual(uploader.main(), 1)

    def test_version_request_rejects_http_errors(self):
        with patch.object(uploader, "get") as request:
            request.return_value.raise_for_status.side_effect = HTTPError("403 Forbidden")
            with self.assertRaises(HTTPError):
                uploader.get_version_from_token("portal.test/api", "sensor", "admin", "password")

    def test_cli_returns_failure_for_network_or_local_errors(self):
        for failure in (Timeout("timeout"), OSError("missing file"), ValueError("invalid version"), EOFError()):
            with self.subTest(failure=type(failure).__name__), \
                 patch.object(uploader, "get_ota_config", return_value=("portal.test/api", "sensor", "admin", "password")), \
                 patch.object(uploader, "start_upload", side_effect=failure), \
                 patch("sys.argv", ["uploader", "firmware.bin"]), \
                 patch("sys.stdout", new_callable=io.StringIO), \
                 patch("sys.stderr", new_callable=io.StringIO):
                self.assertEqual(uploader.main(), 1)

    def test_missing_device_type_is_not_a_success(self):
        with patch.object(uploader, "get_version_from_token", return_value=None), \
             patch.object(uploader, "put") as request, \
             patch("sys.stdout", new_callable=io.StringIO):
            self.assertIs(uploader.start_upload("firmware.bin", "sensor", "portal.test/api", "admin", "password"), False)
            request.assert_not_called()

    def test_cli_returns_success_only_after_upload(self):
        with patch.object(uploader, "get_ota_config", return_value=("portal.test/api", "sensor", "admin", "password")), \
             patch.object(uploader, "start_upload", return_value=True), \
             patch("sys.argv", ["uploader", "firmware.bin"]), \
             patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(uploader.main(), 0)


if __name__ == "__main__":
    unittest.main()

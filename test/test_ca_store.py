import re
import ssl
import tempfile
import unittest
from pathlib import Path

from scripts.build_ca_store import build_ca_store


class CaStoreTest(unittest.TestCase):
    def test_archive_contains_the_trusted_roots_in_der_format(self):
        pem_path = Path(__file__).resolve().parents[1] / "data/provisioning/ca.crt"
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.load_verify_locations(cafile=str(pem_path))
        roots = context.get_ca_certs()
        names = {entry[0][1] for root in roots for entry in root["subject"]
                 if entry[0][0] == "commonName"}
        self.assertTrue({"ISRG Root X1", "ISRG Root X2"}.issubset(names))
        blocks = re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", pem_path.read_text(), re.DOTALL)
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "certs.ar"
            build_ca_store(pem_path, archive)
            data = archive.read_bytes()
            self.assertEqual(data[:8], b"!<arch>\n")
            offset = 8
            for block in blocks:
                header = data[offset:offset + 60]
                self.assertEqual(header[58:], b"`\n")
                length = int(header[48:58])
                offset += 60
                der = data[offset:offset + length]
                self.assertEqual(der, ssl.PEM_cert_to_DER_cert(block))
                offset += length + length % 2
            self.assertEqual(offset, len(data))
            modified = archive.stat().st_mtime_ns
            build_ca_store(pem_path, archive)
            self.assertEqual(archive.stat().st_mtime_ns, modified)

    def test_invalid_or_missing_ca_does_not_replace_an_existing_store(self):
        with tempfile.TemporaryDirectory() as directory:
            pem, archive = Path(directory) / "ca.crt", Path(directory) / "certs.ar"
            archive.write_bytes(b"previous store")
            with self.assertRaises(FileNotFoundError):
                build_ca_store(pem, archive)
            pem.write_text("invalid certificate")
            with self.assertRaises(ssl.SSLError):
                build_ca_store(pem, archive)
            self.assertEqual(archive.read_bytes(), b"previous store")

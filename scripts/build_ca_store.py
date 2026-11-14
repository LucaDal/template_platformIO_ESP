import re
import ssl
from pathlib import Path


def build_ca_store(pem_path: Path, archive_path: Path):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.load_verify_locations(cafile=str(pem_path))
    certificates = re.findall(
        r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
        pem_path.read_text(encoding="ascii"), re.DOTALL,
    )
    archive = bytearray(b"!<arch>\n")
    for index, certificate in enumerate(certificates):
        der = ssl.PEM_cert_to_DER_cert(certificate)
        header = f'{str(index) + ".der/":<16}{0:<12}{0:<6}{0:<6}{"100644":<8}{len(der):<10}`\n'
        archive.extend(header.encode("ascii"))
        archive.extend(der)
        if len(der) % 2:
            archive.extend(b"\n")
    if not archive_path.exists() or archive_path.read_bytes() != archive:
        archive_path.write_bytes(archive)

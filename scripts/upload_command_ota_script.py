import argparse
import base64
import gzip
import json
import os
import sys
from pathlib import Path

from requests import RequestException, get, put

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.project_config import load_dotenv

URL_VERSION = "https://{}/ota/firmware/version"
URL_UPLOAD = "https://{}/ota/firmware"
ENV_FILE_NAME = ".env"

ENV_EMAIL = "OTA_EMAIL"
ENV_PASSWORD = "OTA_PASSWORD"
ENV_DEVICE_TYPE = "OTA_DEVICE_TYPE"
ENV_SERVER = "OTA_SERVER_ADDRESS"


def get_ota_config():
    project_root = Path(__file__).resolve().parent.parent
    env_path = project_root / ENV_FILE_NAME
    dotenv_values = load_dotenv(env_path)
    email = os.environ.get(ENV_EMAIL)
    password = os.environ.get(ENV_PASSWORD)
    device_type_id = os.environ.get(ENV_DEVICE_TYPE)
    server = os.environ.get(ENV_SERVER)

    if not email:
        email = dotenv_values.get(ENV_EMAIL)
    if not password:
        password = dotenv_values.get(ENV_PASSWORD)
    if not device_type_id:
        device_type_id = dotenv_values.get(ENV_DEVICE_TYPE)
    if not server:
        server = dotenv_values.get(ENV_SERVER)

    if not email:
        raise ValueError(f"{ENV_EMAIL} non trovato in env o {env_path}")
    if not password:
        raise ValueError(f"{ENV_PASSWORD} non trovato in env o {env_path}")
    if not device_type_id:
        raise ValueError(f"{ENV_DEVICE_TYPE} non trovato in env o {env_path}")
    if not server:
        raise ValueError(f"{ENV_SERVER} non trovato in env o {env_path}")

    return server, device_type_id, email, password


def build_auth_header(email: str, password: str) -> str:
    credentials = f"{email}:{password}".encode("utf-8")
    return "Basic " + base64.b64encode(credentials).decode("ascii")


def build_headers(device_type_id: str, email: str, password: str) -> dict[str, str]:
    return {
        "Authorization": build_auth_header(email, password),
        "x-device-type-id": device_type_id,
    }


def get_version_from_token(ip, device_type_id, email, password):
    response = get(
        URL_VERSION.format(ip),
        headers=build_headers(device_type_id, email, password),
        timeout=30,
    )
    response.raise_for_status()
    data_token = response.text
    if data_token == "none" or data_token is None:
        return None
    returned = json.loads(data_token)
    if not isinstance(returned, dict):
        raise ValueError("Invalid firmware version response")
    if "error" in returned:
        print(returned["error"])
        return None
    print("current build: {}".format(data_token), flush=True)
    version = returned.get("version")
    if not isinstance(version, str) or not version:
        raise ValueError("Missing firmware version")
    return version


def get_latest_version(current_version: list[int], bump: str = "patch") -> str:
    if not current_version:
        raise ValueError("current_version cannot be empty")
    version = current_version[:]

    if bump == "patch":
        index_to_bump = len(version) - 1
    elif bump == "minor":
        index_to_bump = max(len(version) - 2, 0)
    elif bump == "major":
        index_to_bump = 0
    else:
        raise ValueError(f"unknown bump type: {bump}")

    if version[index_to_bump] >= 255:
        raise ValueError("cannot increment: field already 255")

    version[index_to_bump] += 1
    for i in range(index_to_bump + 1, len(version)):
        version[i] = 0

    return ".".join(str(x) for x in version)


def ask_version_bump(current_version: list[int]) -> str:
    current_str = '.'.join(str(x) for x in current_version)

    def prompt_line(text: str):
        # Force prompt to stderr to avoid interleaving with other stdout messages.
        stream = sys.stderr
        stream.write(text)
        stream.flush()

    prompt_line(f"Current version: {current_str}\n")
    prompt_line("Choose release type:\n")
    prompt_line("  [1] auto patch (default)\n")
    prompt_line("  [2] new minor\n")
    prompt_line("  [3] new major\n")
    prompt_line("  [4] reuse current version\n")
    prompt_line("Selection: ")

    choice = input().strip()

    if choice == "2":
        return "minor"
    if choice == "3":
        return "major"
    if choice == "4":
        return "same"
    return "patch"

def start_upload(firmware_path, device_type_id, ip, email, password, compress=False):
    version = get_version_from_token(ip, device_type_id, email, password)
    if version is None:
        print("No board for device type: [{}]".format(device_type_id))
        return False

    current_int_version = list(map(lambda x: int(x),version.split(".")))
    bump_type = ask_version_bump(current_int_version)

    if bump_type == "same":
        new_version = version
    else:
        try:
            new_version = get_latest_version(current_int_version, bump_type)
        except ValueError as exc:
            print(f"Unable to calculate new version: {exc}")
            return False

    firmware = Path(firmware_path).read_bytes()
    filename = Path(firmware_path).name
    if compress:
        firmware = gzip.compress(firmware, mtime=0)
        filename += ".gz"

    res = put(
        url=URL_UPLOAD.format(ip),
        headers=build_headers(device_type_id, email, password),
        data={'version': new_version},
        files={'file': (filename, firmware, 'application/octet-stream')},
        timeout=60,
    )
    if res.ok:
        result = json.loads(res.text)
        banner = "=" * 55
        print(f"\n{banner}")
        print("OTA UPDATE STATUS: SUCCESS")
        print(f"Message: {result}")
        print(f"{banner}\n")
        return True
    else:
        print("Error on updating: ", res.text)
        return False

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware_path")
    parser.add_argument("--gzip", action="store_true", dest="compress")
    args = parser.parse_args()
    try:
        ip, device_type_id, email, password = get_ota_config()
        print("executing script with: ", ip, device_type_id, flush=True)
        uploaded = start_upload(args.firmware_path, device_type_id, ip, email, password, args.compress)
        return 0 if uploaded else 1
    except (RequestException, OSError, ValueError, EOFError) as error:
        print(f"OTA upload failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

import json
import os
import sys
from pathlib import Path

from requests import get, post

URL_VERSION="https://{}/ota/type/{}/version"
URL_UPLOAD="https://{}/ota/upload"
ENV_FILE_NAME = ".env"


def load_dotenv(env_path: Path) -> dict[str, str]:
    values: dict[str, str] = {}

    if not env_path.is_file():
        return values

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        if key:
            values[key] = value

    return values


def get_ota_config():
    project_root = Path(__file__).resolve().parent.parent
    env_path = project_root / ENV_FILE_NAME
    dotenv_values = load_dotenv(env_path)

    ip = os.environ.get("OTA_SERVER") or os.environ.get("PORTAL_SERVER_IP")
    token = os.environ.get("OTA_DEVICE_TYPE_ID") or os.environ.get("DEVICE_TYPE_ID")

    if not ip:
        ip = dotenv_values.get("OTA_SERVER") or dotenv_values.get("PORTAL_SERVER_IP")
    if not token:
        token = dotenv_values.get("OTA_DEVICE_TYPE_ID") or dotenv_values.get("DEVICE_TYPE_ID")

    if not ip:
        raise ValueError(
            f"PORTAL_SERVER_IP/OTA_SERVER non trovato in env o {env_path}"
        )
    if not token:
        raise ValueError(
            f"DEVICE_TYPE_ID/OTA_DEVICE_TYPE_ID non trovato in env o {env_path}"
        )

    return (ip, token)


def get_version_from_TOKEN(ip, token):
    data_TOKEN = get(URL_VERSION.format(ip,token)).text
    if data_TOKEN == "none" or data_TOKEN is None:
        return None
    returned = json.loads(data_TOKEN)
    if "error" in returned:
        print(returned["error"])
        return None
    print("current build: {}".format(data_TOKEN), flush=True)
    return json.loads(data_TOKEN)["version"]


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

def start_upload(firmware_path, token, ip):
    version = get_version_from_TOKEN(ip,token)
    if version is None:
        print("No board for token: [{}]".format(token))
        return

    current_int_version = list(map(lambda x: int(x),version.split(".")))
    bump_type = ask_version_bump(current_int_version)

    if bump_type == "same":
        new_version = version
    else:
        try:
            new_version = get_latest_version(current_int_version, bump_type)
        except ValueError as exc:
            print(f"Unable to calculate new version: {exc}")
            return

    res = post(
        url=URL_UPLOAD.format(ip),
        data={'token': token, 'version': new_version},
        files= {'file': open(firmware_path,'rb')})
    if res.ok:
        result = json.loads(res.text)["ok"]
        banner = "=" * 55
        print(f"\n{banner}")
        print("OTA UPDATE STATUS: SUCCESS")
        print(f"Message: {result}")
        print(f"{banner}\n")
    else:
        print("Error on updating: ", res.text)

def main():
    ip,token = get_ota_config()
    print("executing script with: " ,ip, token, flush=True)
    start_upload(sys.argv[1],token, ip)


if __name__ == "__main__":
    main()

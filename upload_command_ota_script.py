import json
from requests import get, post
import sys
import re
from pathlib import Path

URL_VERSION="https://{}/ota/type/{}/version"
URL_UPLOAD="https://{}/ota/upload"

def get_address_ip_from_header():
    # cartella dove si trova questo script
    base_dir = Path(__file__).resolve().parent
    header_path = base_dir / "include/secret_data.h"

    if not header_path.is_file():
        raise FileNotFoundError(f"File non trovato: {header_path}")

    text = header_path.read_text(encoding="utf-8")

    # Consente eventuali qualificatori (es. PROGMEM) fra [] e l'assegnazione.
    match_ip = re.search(r'PORTAL_SERVER_IP\s*\[\][^=]*=\s*"([^"]*)"', text)
    match_token = re.search(r'DEVICE_TYPE_ID\s*\[\][^=]*=\s*"([^"]*)"', text)

    if not match_ip:
        raise ValueError("PORTAL_SERVER_IP non trovato in {}".format(header_path))
    if not match_token:
        raise ValueError("DEVICE_TYPE_ID non trovato in {}".format(header_path))
    return (match_ip.group(1),match_token.group(1))


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
    ip,token = get_address_ip_from_header()
    print("executing script with: " ,ip, token, flush=True)
    start_upload(sys.argv[1],token, ip)


if __name__ == "__main__":
    main()

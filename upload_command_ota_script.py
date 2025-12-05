import json
from requests import get, post
import sys
import re
from pathlib import Path

URL_VERSION="http://{}/ota/{}/version"
URL_UPLOAD="http://{}/ota/upload"

def get_address_ip_from_header():
    # cartella dove si trova questo script
    base_dir = Path(__file__).resolve().parent
    header_path = base_dir / "include/secret_data.h"

    text = header_path.read_text(encoding="utf-8")

    match_ip = re.search(r'const\s+char\s*\*\s*PORTAL_SERVER_IP\s*=\s*"([^"]*)"', text)
    match_token = re.search(r'const\s+char\s*\*\s*DEVICE_TYPE_ID\s*=\s*"([^"]*)"', text)

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
    print("current build: {}".format(data_TOKEN))
    return json.loads(data_TOKEN)["version"]


def get_latest_version(current_version: list[int]) -> str:
    if not current_version:
        raise ValueError("current_version non può essere vuota")
    version = current_version[:]

    for i in range(len(version) - 1, -1, -1):
        if version[i] < 255:
            version[i] += 1
            break
        else:
            version[i] = 0
    else:
        raise ValueError("every field was 255, (255.255.255), cannot proceed.")
    return ".".join(str(x) for x in version)

def start_upload(firmware_path, token, ip):
    version = get_version_from_TOKEN(ip,token)
    if version is None:
        print("No board for token: [{}]".format(token))
        return

    current_int_version = list(map(lambda x: int(x),version.split(".")))
    new_version = get_latest_version(current_int_version)

    res = post(
        url=URL_UPLOAD.format(ip),
        data={'token': token, 'version': new_version},
        files= {'file': open(firmware_path,'rb')})
    if res.ok:
        print("\t=== UPDATE DONE===\n")
        print("\t{}\n".format(json.loads(res.text)["ok"]))
        print("\t==================")
    else:
       print("Error on updating: ", res.text)

def main():
    ip,token = get_address_ip_from_header()
    print("executing script with: " ,ip, token)
    start_upload(sys.argv[1],token, ip)


if __name__ == "__main__":
    main()


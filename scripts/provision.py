#!/usr/bin/env python3
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

REQUIRED_FIELDS = ("device_id", "env", "cert", "key", "ca")


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(1)


def require_absolute_path(path_value: str, field: str) -> Path:
    p = Path(path_value).expanduser()
    if not p.is_absolute():
        fail(f"Field '{field}' must be an absolute path: {path_value}")
    if not p.exists():
        fail(f"File for '{field}' does not exist: {p}")
    if not p.is_file():
        fail(f"Path for '{field}' is not a file: {p}")
    return p


def load_device(config_path: Path, device_id: str) -> dict:
    if not config_path.exists() or not config_path.is_file():
        fail(f"Config file not found: {config_path}")

    try:
        data = json.loads(config_path.read_text(encoding="utf-8"))
    except Exception as exc:
        fail(f"Invalid JSON config '{config_path}': {exc}")

    devices = data.get("devices")
    if not isinstance(devices, list):
        fail("JSON must contain a 'devices' array")

    for dev in devices:
        if isinstance(dev, dict) and dev.get("device_id") == device_id:
            for field in REQUIRED_FIELDS:
                if field not in dev or not str(dev[field]).strip():
                    fail(f"Device '{device_id}' missing required field '{field}'")
            return dev

    fail(f"Device '{device_id}' not found in config {config_path}")
    return {}


def prepare_data_folder(project_dir: Path, device: dict) -> None:
    data_dir = project_dir / "data"
    provisioning_dir = data_dir / "provisioning"
    device_dir = data_dir / "device"

    provisioning_dir.mkdir(parents=True, exist_ok=True)
    device_dir.mkdir(parents=True, exist_ok=True)

    keep_files = {"fw_version.txt": "0.0.0\n", "properties.json": "{}\n"}
    for name, default_content in keep_files.items():
        target = device_dir / name
        if not target.exists():
            target.write_text(default_content, encoding="utf-8")

    for child in provisioning_dir.iterdir():
        if child.is_file():
            child.unlink()

    cert_src = require_absolute_path(str(device["cert"]), "cert")
    key_src = require_absolute_path(str(device["key"]), "key")
    ca_src = require_absolute_path(str(device["ca"]), "ca")

    (provisioning_dir / "device_id.txt").write_text(
        f"{device['device_id']}\n", encoding="utf-8"
    )
    shutil.copy2(cert_src, provisioning_dir / "client.crt")
    shutil.copy2(key_src, provisioning_dir / "client.key")
    shutil.copy2(ca_src, provisioning_dir / "ca.crt")


def run_pio(project_dir: Path, env_name: str, skip_firmware_upload: bool) -> None:
    base_cmd = ["pio", "run", "-e", env_name]

    steps = [base_cmd + ["-t", "buildfs"], base_cmd + ["-t", "uploadfs"]]
    if not skip_firmware_upload:
        steps.append(base_cmd + ["-t", "upload"])

    for cmd in steps:
        print("+", " ".join(cmd))
        result = subprocess.run(cmd, cwd=project_dir)
        if result.returncode != 0:
            fail(f"Command failed ({result.returncode}): {' '.join(cmd)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Provision one device from JSON inventory and upload FS via PlatformIO."
    )
    parser.add_argument("device_id", help="Device identifier from config JSON")
    parser.add_argument(
        "--config",
        default="provisioning/devices.json",
        help="Absolute or project-relative path to devices JSON (default: provisioning/devices.json)",
    )
    parser.add_argument(
        "--project-dir",
        default=".",
        help="PlatformIO project directory (default: current directory)",
    )
    parser.add_argument(
        "--skip-firmware-upload",
        action="store_true",
        help="Only upload filesystem (buildfs/uploadfs), skip firmware upload",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    project_dir = Path(args.project_dir).expanduser().resolve()
    if not (project_dir / "platformio.ini").exists():
        fail(f"platformio.ini not found in project dir: {project_dir}")

    config_path = Path(args.config).expanduser()
    if not config_path.is_absolute():
        config_path = (project_dir / config_path).resolve()

    device = load_device(config_path, args.device_id)
    env_name = str(device["env"]).strip()

    prepare_data_folder(project_dir, device)

    print(f"Prepared provisioning files for device_id={device['device_id']} env={env_name}")
    run_pio(project_dir, env_name, args.skip_firmware_upload)
    print("Provisioning complete.")


if __name__ == "__main__":
    main()

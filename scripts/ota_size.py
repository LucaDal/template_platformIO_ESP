import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess


def write_flash_report(env):
    """Record the same Flash counters PlatformIO prints for this upload build."""
    firmware = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    elf = env.subst("$BUILD_DIR/${PROGNAME}.elf")
    command = env.get("SIZECHECKCMD") or "$SIZETOOL -B -d $SOURCES"
    if isinstance(command, str):
        command = shlex.split(command)
    command = env.subst([argument.replace("$SOURCES", elf) for argument in command])
    environment = dict(os.environ, PATH=str(env["ENV"]["PATH"]))
    output = subprocess.check_output(command, env=environment, text=True)
    pattern = re.compile(env.get("SIZEPROGREGEXP") or r"^(\d+)\s+(\d+)\s+\d+\s")
    matches = [match for line in output.splitlines() if (match := pattern.search(line.strip()))]
    if not matches:
        raise ValueError("Cannot determine PlatformIO Flash usage for the OTA size estimate")
    used = sum(int(value) for match in matches for value in match.groups())
    total = int(env.BoardConfig().get("upload.maximum_size", 0))
    if not 0 <= used <= total or total <= 0:
        raise ValueError("Invalid PlatformIO Flash values for the OTA size estimate")
    report = {"flash_used": used, "flash_total": total,
              "firmware_sha256": hashlib.sha256(firmware.read_bytes()).hexdigest()}
    Path(str(firmware) + ".ota-size.json").write_text(json.dumps(report), encoding="utf-8")


def report_upload_size(firmware_path, original, upload, compressed):
    """Estimate OTA headroom from the current build, not remote device telemetry."""
    print(f"Firmware BIN: {len(original)} bytes ({len(original) / 1024:.1f} KiB)", flush=True)
    if not compressed:
        return True
    print(f"Firmware GZIP: {len(upload)} bytes ({len(upload) / 1024:.1f} KiB)", flush=True)
    try:
        report = json.loads(Path(str(firmware_path) + ".ota-size.json").read_text(encoding="utf-8"))
        if not isinstance(report, dict):
            raise ValueError("Invalid size report")
        used, total = report.get("flash_used"), report.get("flash_total")
        if (type(used) is not int or type(total) is not int or not 0 <= used <= total or total <= 0 or
                report.get("firmware_sha256") != hashlib.sha256(original).hexdigest()):
            raise ValueError("Invalid or stale size report")
    except (OSError, ValueError):
        print("OTA size check: UNAVAILABLE (PlatformIO Flash values missing or stale).", flush=True)
        return True

    available = total - used
    margin = available - len(upload)
    print(f"PlatformIO Flash: used {used} bytes from {total} bytes", flush=True)
    print(f"OTA budget (estimated): {available} bytes", flush=True)
    print("Estimate assumes installed firmware and flash layout match this build; sector alignment is not included.",
          flush=True)
    if margin < 0:
        print(f"OTA size check: INSUFFICIENT (exceeds estimated budget by {-margin} bytes). Upload cancelled.",
              flush=True)
        return False
    print(f"OTA size check: OK (estimated margin: {margin} bytes)", flush=True)
    return True

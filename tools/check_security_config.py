#!/usr/bin/env python3
"""Fail CI on Wi-Fi secrets in committed ESP-IDF configs; report at-rest risks.

This is a narrow source/configuration guard, not a secret scanner for all Git
history, a runtime NVS-encryption test, or a substitute for hardware review.
"""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CONFIGS = (
    ROOT / "esp32/sdkconfig.defaults",
    ROOT / "esp32/sdkconfig.verified",
)
SENSITIVE = ("CONFIG_CSIRADAR_WIFI_PASSWORD", "CONFIG_CSIRADAR_WIFI_SSID")


def config_value(text: str, key: str) -> str | None:
    match = re.search(rf"(?m)^\s*{re.escape(key)}=(.*)$", text)
    return match.group(1).strip() if match else None


def is_nonempty_credential(value: str | None) -> bool:
    if value is None:
        return False
    value = value.strip()
    return value not in ('""', "''", "")


def main() -> int:
    failure = False
    for path in CONFIGS:
        if not path.is_file():
            print(f"FAIL: required config missing: {path.relative_to(ROOT)}")
            failure = True
            continue
        text = path.read_text(encoding="utf-8")
        for key in SENSITIVE:
            if is_nonempty_credential(config_value(text, key)):
                # NEVER include value in logs, even when failing a CI job.
                print(f"FAIL: {key} is populated in {path.relative_to(ROOT)}")
                failure = True

    # The tracked sdkconfig.verified is build evidence; it does not prove the
    # attached ESP32 has these settings or that its flash contains no secrets.
    verified = ROOT / "esp32/sdkconfig.verified"
    if verified.is_file():
        data = verified.read_text(encoding="utf-8")
        if config_value(data, "CONFIG_NVS_ENCRYPTION") != "y":
            print("REVIEW: tracked configuration does not enable NVS encryption")
        if config_value(data, "CONFIG_FLASH_ENCRYPTION_ENABLED") != "y":
            print("REVIEW: tracked configuration does not enable flash encryption")
        if config_value(data, "CONFIG_SECURE_BOOT") != "y":
            print("REVIEW: tracked configuration does not enable secure boot")

    if failure:
        print("Security-config guard failed. Do not include credentials in CI output.")
        return 1
    print("PASS: no populated Wi-Fi credentials in the checked ESP-IDF config files")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Generate a throwaway esphome/secrets.yaml for CI.

Real deployments use a git-ignored secrets.yaml; CI has none, so this writes a
placeholder file. Only the API encryption key must be well-formed (valid base64
of 32 bytes) or `esphome config` rejects it; the rest can be dummy strings.
Never use this file to flash a real node.
"""

import base64
import os
import pathlib

esphome_dir = pathlib.Path(__file__).resolve().parent.parent  # -> esphome/
api_key = base64.b64encode(os.urandom(32)).decode()

(esphome_dir / "secrets.yaml").write_text(
    f'wifi_ssid: "ci"\n'
    f'wifi_password: "ci-password"\n'
    f'ap_password: "ci-ap-password"\n'
    f'api_encryption_key: "{api_key}"\n'
    f'ota_password: "ci-ota-password"\n'
    f'mqtt_broker: "127.0.0.1"\n'
    f'mqtt_username: "ci"\n'
    f'mqtt_password: "ci"\n'
)

print(f"Wrote {esphome_dir / 'secrets.yaml'} (CI placeholder)")

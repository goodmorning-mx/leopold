#!/usr/bin/env python3
"""Run Leopold's allowlisted computer controls on macOS."""

from __future__ import annotations

import json
import re
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / "device" / "include" / "leopold_config.h"
SCREENSHOT_DIR = Path.home() / "Desktop" / "Leopold Screenshots"
POLL_SECONDS = 1.0
TLS_CA_FILE = "/etc/ssl/cert.pem"


def read_define(source: str, name: str) -> str:
    match = re.search(rf'^#define\s+{re.escape(name)}\s+"([^"]+)"', source, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Missing {name} in {CONFIG_PATH}")
    return match.group(1)


def load_config() -> tuple[str, str, str]:
    source = CONFIG_PATH.read_text(encoding="utf-8")
    return (
        read_define(source, "LEOPOLD_API_BASE_URL").rstrip("/"),
        read_define(source, "LEOPOLD_DEVICE_ID"),
        read_define(source, "LEOPOLD_DEVICE_SECRET"),
    )


def run_osascript(script: str) -> None:
    subprocess.run(["/usr/bin/osascript", "-e", script], check=True)


def execute(component_id: str) -> str:
    if component_id == "computer-volume-down":
        run_osascript("set volume output volume ((output volume of (get volume settings)) - 10)")
        return "volume down"
    if component_id == "computer-volume-up":
        run_osascript("set volume output volume ((output volume of (get volume settings)) + 10)")
        return "volume up"
    if component_id == "computer-volume-mute":
        run_osascript("set volume output muted not (output muted of (get volume settings))")
        return "mute toggled"
    if component_id == "computer-screenshot":
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        destination = SCREENSHOT_DIR / f"Leopold_{stamp}.png"
        subprocess.run(["/usr/sbin/screencapture", "-x", str(destination)], check=True)
        return f"screenshot saved to {destination}"
    raise ValueError(f"Unsupported command: {component_id}")


def fetch_commands(base_url: str, device_id: str, secret: str, after: datetime) -> list[dict]:
    query = urllib.parse.urlencode({"after": after.isoformat()})
    request = urllib.request.Request(
        f"{base_url}/v1/device/computer-commands?{query}",
        headers={
            "X-Leopold-Device-ID": device_id,
            "Authorization": f"Bearer {secret}",
        },
    )
    context = ssl.create_default_context(cafile=TLS_CA_FILE)
    with urllib.request.urlopen(request, timeout=10, context=context) as response:
        return json.load(response)


def main() -> int:
    if sys.platform != "darwin":
        print("Leopold computer controls require macOS.", file=sys.stderr)
        return 2
    base_url, device_id, secret = load_config()
    after = datetime.now(timezone.utc)
    seen: set[str] = set()
    print(f"Leopold Mac companion connected for {device_id}. Press Ctrl+C to stop.")
    while True:
        try:
            commands = fetch_commands(base_url, device_id, secret, after)
            for command in commands:
                received_at = datetime.fromisoformat(command["received_at"].replace("Z", "+00:00"))
                after = max(after, received_at)
                if command["event_id"] in seen:
                    continue
                seen.add(command["event_id"])
                print(execute(command["component_id"]))
            if len(seen) > 500:
                seen.clear()
        except (OSError, ValueError, KeyError, urllib.error.URLError) as error:
            print(f"Companion error: {error}", file=sys.stderr)
        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nLeopold Mac companion stopped.")

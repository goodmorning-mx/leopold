#!/usr/bin/env python3
"""Run Leopold's allowlisted computer controls over USB or Device Cloud."""

from __future__ import annotations

import json
import argparse
import fcntl
import glob
import re
import ssl
import serial
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
LOCK_PATH = Path("/tmp/leopold-direct-usb-companion.lock")
POLL_SECONDS = 1.0
USB_POLL_SECONDS = 0.01
TLS_CA_FILE = "/etc/ssl/cert.pem"
USB_BAUD_RATE = 115200
USB_COMMAND_PREFIX = "LEOPOLD_CMD:"


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


def run_windows_key(virtual_key: int) -> None:
    import ctypes

    user32 = ctypes.windll.user32
    user32.keybd_event(virtual_key, 0, 0, 0)
    user32.keybd_event(virtual_key, 0, 2, 0)


def run_linux(command: list[str]) -> None:
    subprocess.run(command, check=True)


def execute(component_id: str) -> str:
    if sys.platform == "darwin":
        if component_id == "computer-volume-down":
            run_osascript("set volume output volume ((output volume of (get volume settings)) - 10)")
            return "volume down"
        if component_id == "computer-volume-up":
            run_osascript("set volume output volume ((output volume of (get volume settings)) + 10)")
            return "volume up"
        if component_id == "computer-volume-mute":
            run_osascript("set volume output muted not (output muted of (get volume settings))")
            return "mute toggled"
    elif sys.platform == "win32":
        if component_id == "computer-volume-down":
            run_windows_key(0xAE)
            return "volume down"
        if component_id == "computer-volume-up":
            run_windows_key(0xAF)
            return "volume up"
        if component_id == "computer-volume-mute":
            run_windows_key(0xAD)
            return "mute toggled"
    elif sys.platform.startswith("linux"):
        if component_id == "computer-volume-down":
            run_linux(["pactl", "set-sink-volume", "@DEFAULT_SINK@", "-10%"])
            return "volume down"
        if component_id == "computer-volume-up":
            run_linux(["pactl", "set-sink-volume", "@DEFAULT_SINK@", "+10%"])
            return "volume up"
        if component_id == "computer-volume-mute":
            run_linux(["pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle"])
            return "mute toggled"
    if component_id == "computer-screenshot":
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        destination = SCREENSHOT_DIR / f"Leopold_{stamp}.png"
        if sys.platform == "darwin":
            subprocess.run(["/usr/sbin/screencapture", "-x", str(destination)], check=True)
        elif sys.platform == "win32":
            script = (
                "Add-Type -AssemblyName System.Windows.Forms,System.Drawing; "
                "$b=[Windows.Forms.Screen]::PrimaryScreen.Bounds; "
                "$i=New-Object Drawing.Bitmap($b.Width,$b.Height); "
                "$g=[Drawing.Graphics]::FromImage($i); "
                "$g.CopyFromScreen($b.Location,[Drawing.Point]::Empty,$b.Size); "
                f"$i.Save('{destination.as_posix()}',[Drawing.Imaging.ImageFormat]::Png)"
            )
            subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script], check=True)
        elif sys.platform.startswith("linux"):
            for program in ("gnome-screenshot", "scrot", "import"):
                if subprocess.run(["which", program], capture_output=True).returncode == 0:
                    subprocess.run([program, str(destination)], check=True)
                    break
            else:
                raise RuntimeError("No supported Linux screenshot utility is installed")
        else:
            raise RuntimeError(f"Unsupported host platform: {sys.platform}")
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


def find_usb_port() -> str | None:
    candidates = sorted(
        glob.glob("/dev/cu.usbmodem*") +
        glob.glob("/dev/ttyACM*") +
        glob.glob("/dev/tty.usbmodem*")
    )
    return candidates[0] if candidates else None


def open_usb_port(requested: str | None) -> serial.Serial | None:
    port = requested or find_usb_port()
    if port is None:
        return None
    try:
        return serial.Serial(port, USB_BAUD_RATE, timeout=0.2)
    except serial.SerialException as error:
        print(f"USB companion unavailable: {error}", file=sys.stderr)
        return None


def parse_usb_command(line: bytes) -> str | None:
    text = line.decode("utf-8", errors="replace").strip()
    if not text.startswith(USB_COMMAND_PREFIX):
        return None
    component_id = text.removeprefix(USB_COMMAND_PREFIX)
    return component_id if component_id in {
        "computer-volume-down",
        "computer-volume-mute",
        "computer-volume-up",
        "computer-screenshot",
    } else None


def main() -> int:
    LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    lock_file = LOCK_PATH.open("w")
    try:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print("Leopold Mac companion is already running.", file=sys.stderr)
        return 0
    parser = argparse.ArgumentParser()
    parser.add_argument("--usb-port", help="USB serial port for Leopold")
    args = parser.parse_args()
    usb = open_usb_port(args.usb_port)
    base_url = device_id = secret = None
    if usb is None:
        base_url, device_id, secret = load_config()
    after = datetime.now(timezone.utc)
    seen: set[str] = set()
    if usb is not None:
        print(f"Leopold Mac companion connected over USB ({usb.port}). Press Ctrl+C to stop.")
    else:
        print(f"Leopold Mac companion connected through Device Cloud for {device_id}. Press Ctrl+C to stop.")
    while True:
        try:
            if usb is None and args.usb_port is not None:
                usb = open_usb_port(args.usb_port)
                if usb is not None:
                    base_url = device_id = secret = None
                    print(f"Leopold companion reconnected over USB ({usb.port}).")
            if usb is not None:
                component_id = parse_usb_command(usb.readline())
                if component_id is not None:
                    print(execute(component_id))
            else:
                if base_url is None:
                    base_url, device_id, secret = load_config()
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
        except (OSError, ValueError, KeyError, serial.SerialException, urllib.error.URLError) as error:
            print(f"Companion error: {error}", file=sys.stderr)
            if usb is not None:
                usb.close()
                usb = None
        time.sleep(USB_POLL_SECONDS if usb is not None else POLL_SECONDS)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nLeopold Mac companion stopped.")

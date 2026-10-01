"""Copy canonical shared assets to the two build consumers."""

from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "assets"
DESTINATIONS = (ROOT / "device" / "assets", ROOT / "frontend" / "public" / "firmware-assets")


def sync() -> None:
    for destination in DESTINATIONS:
        destination.mkdir(parents=True, exist_ok=True)
        for source in SOURCE.glob("*.bin"):
            shutil.copyfile(source, destination / source.name)


if __name__ == "__main__":
    sync()

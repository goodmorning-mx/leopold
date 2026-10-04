"""Copy canonical shared assets to the two build consumers."""

from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "assets"
DESTINATIONS = (
    ROOT / "backend" / "assets",
    ROOT / "device" / "assets",
    ROOT / "frontend" / "public" / "firmware-assets",
)
CONTRACTS = ("firmware-ui.json", "interaction.json", "typography.json", "persistence.json", "asset-manifest.json", "rendering.json", "reference-manifest.json", "reference-states.json")


def sync() -> None:
    for destination in DESTINATIONS:
        destination.mkdir(parents=True, exist_ok=True)
        for source in SOURCE.iterdir():
            if not source.is_file() or source.suffix not in {".bin", ".png"}:
                continue
            shutil.copyfile(source, destination / source.name)
    shutil.copyfile(
        ROOT / "shared" / "asset-manifest.json",
        ROOT / "backend" / "assets" / "asset-manifest.json",
    )
    for destination in DESTINATIONS:
        shutil.copyfile(ROOT / "shared" / "reference-manifest.json", destination / "reference-manifest.json")
    contract_destination = ROOT / "frontend" / "src" / "shared"
    contract_destination.mkdir(parents=True, exist_ok=True)
    for name in CONTRACTS:
        shutil.copyfile(ROOT / "shared" / name, contract_destination / name)


if __name__ == "__main__":
    sync()

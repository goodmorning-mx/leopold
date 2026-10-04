"""Verify that generated consumer assets still match shared/assets."""

from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "assets"
DESTINATIONS = (
    ROOT / "backend" / "assets",
    ROOT / "device" / "assets",
    ROOT / "frontend" / "public" / "firmware-assets",
)
CONTRACT_SOURCE = ROOT / "shared"
CONTRACT_DESTINATION = ROOT / "frontend" / "src" / "shared"
CONTRACTS = ("firmware-ui.json", "interaction.json", "typography.json", "persistence.json", "asset-manifest.json", "rendering.json", "reference-manifest.json", "reference-states.json")

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

errors = []
assets = [source for source in sorted(SOURCE.iterdir()) if source.is_file() and source.suffix in {".bin", ".png"}]
for source in assets:
    for destination in DESTINATIONS:
        candidate = destination / source.name
        if not candidate.is_file() or digest(source) != digest(candidate):
            errors.append(f"{candidate} does not match {source}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
for name in CONTRACTS:
    source = CONTRACT_SOURCE / name
    candidate = CONTRACT_DESTINATION / name
    if not candidate.is_file() or digest(source) != digest(candidate):
        errors.append(f"{candidate} does not match {source}")
backend_manifest = ROOT / "backend" / "assets" / "asset-manifest.json"
if not backend_manifest.is_file() or digest(CONTRACT_SOURCE / "asset-manifest.json") != digest(backend_manifest):
    errors.append(f"{backend_manifest} does not match {CONTRACT_SOURCE / 'asset-manifest.json'}")
for destination in DESTINATIONS:
    candidate = destination / "reference-manifest.json"
    if not candidate.is_file() or digest(CONTRACT_SOURCE / "reference-manifest.json") != digest(candidate):
        errors.append(f"{candidate} does not match {CONTRACT_SOURCE / 'reference-manifest.json'}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f"Shared asset outputs valid: {len(assets)} assets")

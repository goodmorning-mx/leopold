"""Verify that generated consumer assets still match shared/assets."""

from pathlib import Path
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "shared" / "assets"
DESTINATIONS = (ROOT / "device" / "assets", ROOT / "frontend" / "public" / "firmware-assets")

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

errors = []
for source in sorted(SOURCE.glob("*.bin")):
    for destination in DESTINATIONS:
        candidate = destination / source.name
        if not candidate.is_file() or digest(source) != digest(candidate):
            errors.append(f"{candidate} does not match {source}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f"Shared asset outputs valid: {len(list(SOURCE.glob('*.bin')))} assets")

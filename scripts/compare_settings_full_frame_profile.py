"""Write lossless full-frame Settings-vs-reference diagnostics without changing the gate."""
from __future__ import annotations

import json
import sys
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
REFS = {
    "neutral": "settings-neutral-466.png",
    "wifi-pressed": "settings-wifi-pressed-466.png",
    "bluetooth-pressed": "settings-bluetooth-pressed-466.png",
    "display-pressed": "settings-display-pressed-466.png",
    "sound-pressed": "settings-sound-pressed-466.png",
}

def main() -> None:
    capture_dir, output_dir = (Path(value).resolve() for value in sys.argv[1:3])
    output_dir.mkdir(parents=True, exist_ok=True)
    result = {"dimensions": [466, 466], "comparison": "full-frame RGB diagnostic; legacy full-row differences are not relabeled as passes", "states": {}}
    for state, filename in REFS.items():
        reference = Image.open(ROOT / "shared/references/phase1-settings" / filename).convert("RGB")
        capture = Image.open(capture_dir / f"settings-{state}.png").convert("RGB")
        diff = ImageChops.difference(reference, capture)
        diff.save(output_dir / f"{state}-reference-difference.png", format="PNG")
        values = [max(abs(reference.getpixel((x, y))[i] - capture.getpixel((x, y))[i]) for i in range(3)) for y in range(466) for x in range(466)]
        result["states"][state] = {"reference": str((ROOT / "shared/references/phase1-settings" / filename).relative_to(ROOT)), "capture": str((capture_dir / f"settings-{state}.png").relative_to(ROOT)), "pixels": 466 * 466, "pixels_over_delta_0": sum(value != 0 for value in values), "maximum_channel_delta": max(values), "mean_channel_delta": sum(values) / len(values), "passed": False, "difference": str((output_dir / f"{state}-reference-difference.png").relative_to(ROOT))}
    (output_dir / "full-frame-visual-diagnostic.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"output": str(output_dir), "states": list(result["states"])}))

if __name__ == "__main__":
    main()

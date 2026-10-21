#!/usr/bin/env python3
"""Offline firmware provenance/package gate; never opens serial or uses the network."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

try:
    from .flash_firmware import load_package, sha256
except ImportError:
    from flash_firmware import load_package, sha256


def git_provenance(repo: Path, source: str, head: str, base: str | None) -> dict[str, object]:
    for value in (source, head, *([base] if base else [])):
        if not re.fullmatch(r"[0-9a-f]{40}", value):
            raise ValueError("provenance requires full 40-character commit IDs")
    result = subprocess.run(["git", "-C", str(repo), "show", "-s", "--format=%H%n%P", source],
                            capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise ValueError("source commit unavailable in local Git; fetch the verified PR merge ref before this offline gate")
    lines = result.stdout.strip().splitlines()
    parents = lines[1].split() if len(lines) > 1 else []
    if lines[0] != source:
        raise ValueError("resolved source commit differs")
    if source != head:
        if base is None or parents != [base, head]:
            raise ValueError("PR merge parents do not match expected base and PR head")
    elif base is not None:
        raise ValueError("a direct-head build must not be presented as a PR merge build")
    return {"source_commit": source, "pr_head": head, "expected_base": base, "actual_parents": parents}


def verify_archive(archive: Path, package: Path, expected_digest: str) -> dict[str, object]:
    if not re.fullmatch(r"[0-9a-f]{64}", expected_digest) or sha256(archive) != expected_digest:
        raise ValueError("artifact ZIP digest mismatch")
    expected = {"SHA256SUMS.txt"} | {line.split("  ", 1)[1] for line in
                (package / "SHA256SUMS.txt").read_text().splitlines() if line.strip()}
    with zipfile.ZipFile(archive) as stream:
        names = stream.namelist()
        if len(names) != len(set(names)) or set(names) != expected:
            raise ValueError("ZIP entries must exactly match the verified flat package without duplicates")
        for name in names:
            if stream.getinfo(name).file_size != (package / name).stat().st_size:
                raise ValueError(f"ZIP/unpacked size mismatch: {name}")
            digest = hashlib.sha256()
            with stream.open(name) as member:
                for chunk in iter(lambda: member.read(1024 * 1024), b""):
                    digest.update(chunk)
            if digest.hexdigest() != sha256(package / name):
                raise ValueError(f"ZIP/unpacked content mismatch: {name}")
    return {"zip_sha256": expected_digest, "files_verified": len(expected)}


def verify(package: Path, repo: Path, expected_source: str, head: str, base: str | None,
           asset_manifest: Path, archive: Path, digest: str) -> dict[str, object]:
    report: dict[str, object] = {"passed": False, "network_access": False, "serial_access": False, "checks": []}
    checks = report["checks"]
    stage = "package-integrity"
    try:
        manifest = load_package(package)
        checks.append({"gate": stage, "passed": True, "manifest_sha256": sha256(package / "manifest.json")})
        stage = "source-identity"
        if manifest["source_commit"] != expected_source:
            raise ValueError("package is stale or differs from expected source commit")
        checks.append({"gate": stage, "passed": True,
                       **git_provenance(repo, expected_source, head, base)})
        stage = "shared-asset-identity"
        assets = json.loads(asset_manifest.read_text())
        asset_hash = hashlib.sha256(json.dumps(assets, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        if manifest.get("asset_manifest_sha256") != asset_hash:
            raise ValueError("package/shared asset manifest mismatch")
        # Match the bytes consumed by each synchronized runtime, not just its symbolic manifest.
        assets_root = asset_manifest.parent / "assets"
        runtime_root = asset_manifest.parent.parent
        for entry in assets["assets"].values():
            filename = entry["file"]
            canonical = assets_root / filename
            for destination in (runtime_root / "frontend/public/firmware-assets",
                                runtime_root / "backend/assets", runtime_root / "device/assets"):
                if sha256(destination / filename) != sha256(canonical):
                    raise ValueError(f"runtime asset mismatch: {destination.name}/{filename}")
        checks.append({"gate": stage, "passed": True, "asset_manifest_sha256": asset_hash})
        stage = "archive-identity"
        checks.append({"gate": stage, "passed": True, **verify_archive(archive, package, digest)})
        report.update(passed=True, source_commit=expected_source, ui_revision=manifest["ui_revision"],
                      chip=manifest["chip"], flash_size_bytes=manifest["flash_size_bytes"],
                      normal_update=manifest["normal_update"], images=manifest["images"],
                      hardware_prerequisites="not checked: fresh device identification, running revision, backup, and partition equality remain required")
    except (SystemExit, OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile, subprocess.SubprocessError) as exc:
        checks.append({"gate": stage, "passed": False, "error": str(exc)})
        report["first_failed_gate"] = stage
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--pr-head", required=True)
    parser.add_argument("--expected-base")
    parser.add_argument("--asset-manifest", type=Path, required=True)
    parser.add_argument("--zip", dest="archive", type=Path, required=True)
    parser.add_argument("--zip-sha256", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.report.exists():
        parser.error("report exists; use a new evidence path")
    result = verify(args.package, args.repo, args.source_commit, args.pr_head, args.expected_base,
                    args.asset_manifest, args.archive, args.zip_sha256)
    with args.report.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"passed": result["passed"], "report": str(args.report),
                      "first_failed_gate": result.get("first_failed_gate")}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

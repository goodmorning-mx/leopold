"""Bounded, content-verified persistent cache used by isolated asset fixtures.

Network transport is supplied by the trusted-origin adapter; this class never
chooses an origin or receives device credentials. Cache activation is atomic.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import tempfile
import time
from contextlib import contextmanager
from pathlib import Path
from typing import Callable, Iterable, Iterator

_KEY = re.compile(r"^[a-z0-9][a-z0-9._-]{0,95}$")


class AssetCacheError(RuntimeError):
    pass


class VerifiedAssetCache:
    def __init__(self, root: Path, max_bytes: int) -> None:
        if max_bytes < 1:
            raise ValueError("max_bytes must be positive")
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)
        self.index_path = root / "index.json"
        self.max_bytes = max_bytes
        self._leases: dict[str, int] = {}
        self.index = self._read_index()

    def _read_index(self) -> dict:
        try:
            value = json.loads(self.index_path.read_text())
            return value if isinstance(value, dict) and value.get("schema") == 1 else {"schema": 1, "items": {}, "selection": {}}
        except (FileNotFoundError, ValueError, OSError):
            return {"schema": 1, "items": {}, "selection": {}}

    def _save(self) -> None:
        fd, name = tempfile.mkstemp(prefix="index-", dir=self.root)
        try:
            with os.fdopen(fd, "w") as stream:
                json.dump(self.index, stream, sort_keys=True)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(name, self.index_path)
        finally:
            Path(name).unlink(missing_ok=True)

    def select(self, slot: str, key: str) -> None:
        if key not in self.index["items"]:
            raise AssetCacheError("selection is not cached")
        self.index["selection"][slot] = key
        self._save()

    def selected(self, slot: str) -> Path | None:
        key = self.index["selection"].get(slot)
        record = self.index["items"].get(key)
        path = self.root / record["file"] if record else None
        if not path or not path.is_file() or path.stat().st_size != record["bytes"]:
            return None
        if hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]:
            return None
        return path

    @contextmanager
    def lease(self, key: str) -> Iterator[Path]:
        record = self.index["items"].get(key)
        if not record:
            raise AssetCacheError("asset is not cached")
        path = self.root / record["file"]
        if not path.is_file():
            raise AssetCacheError("cached asset is missing")
        self._leases[key] = self._leases.get(key, 0) + 1
        record["last_used"] = time.time()
        try:
            yield path
        finally:
            self._leases[key] -= 1
            if not self._leases[key]:
                del self._leases[key]

    def _evict_for(self, needed: int, incoming_key: str) -> None:
        total = sum(int(v["bytes"]) for v in self.index["items"].values())
        pinned = set(self.index["selection"].values()) | set(self._leases)
        candidates = sorted(
            ((k, v) for k, v in self.index["items"].items() if k not in pinned and k != incoming_key),
            key=lambda pair: (float(pair[1].get("last_used", 0)), pair[0]),
        )
        for key, record in candidates:
            if total + needed <= self.max_bytes:
                break
            (self.root / record["file"]).unlink(missing_ok=True)
            total -= int(record["bytes"])
            del self.index["items"][key]
        if total + needed > self.max_bytes:
            raise AssetCacheError("storage exhausted: pinned or active assets fill cache")

    def install(
        self,
        key: str,
        chunks: Iterable[bytes],
        *,
        expected_bytes: int,
        sha256: str,
        catalog_version: int,
        supported_version: int,
        timeout_seconds: float = 5.0,
    ) -> Path:
        if not _KEY.fullmatch(key):
            raise AssetCacheError("invalid logical asset key")
        if catalog_version != supported_version:
            raise AssetCacheError("incompatible catalog version")
        if expected_bytes < 1 or not re.fullmatch(r"[a-f0-9]{64}", sha256):
            raise AssetCacheError("invalid immutable asset metadata")
        fd, temporary = tempfile.mkstemp(prefix="asset-stage-", dir=self.root)
        digest = hashlib.sha256()
        count = 0
        started = time.monotonic()
        try:
            with os.fdopen(fd, "wb") as target:
                for chunk in chunks:
                    if time.monotonic() - started > timeout_seconds:
                        raise AssetCacheError("asset transfer timed out")
                    if not isinstance(chunk, bytes):
                        raise AssetCacheError("asset stream yielded non-bytes")
                    count += len(chunk)
                    if count > expected_bytes:
                        raise AssetCacheError("asset length exceeded manifest")
                    digest.update(chunk)
                    target.write(chunk)
                target.flush()
                os.fsync(target.fileno())
            if count != expected_bytes:
                raise AssetCacheError("truncated asset")
            if digest.hexdigest() != sha256:
                raise AssetCacheError("asset SHA-256 mismatch")
            self._evict_for(expected_bytes, key)
            filename = f"{key}-{sha256[:16]}.bin"
            destination = self.root / filename
            os.replace(temporary, destination)
            old = self.index["items"].get(key)
            self.index["items"][key] = {
                "file": filename, "bytes": count, "sha256": sha256,
                "catalog_version": catalog_version, "last_used": time.time(),
            }
            self._save()
            if old and old["file"] != filename and self._leases.get(key, 0) == 0 and key not in self.index["selection"].values():
                (self.root / old["file"]).unlink(missing_ok=True)
            return destination
        except Exception:
            Path(temporary).unlink(missing_ok=True)
            raise

    def fetch_and_install(
        self,
        key: str,
        fetcher: Callable[[float], Iterable[bytes]],
        *,
        expected_bytes: int,
        sha256: str,
        catalog_version: int,
        supported_version: int,
        attempts: int = 3,
        timeout_seconds: float = 5.0,
    ) -> Path:
        if attempts < 1 or attempts > 5 or timeout_seconds <= 0:
            raise AssetCacheError("retry/timeout policy is outside bounded limits")
        last_error: Exception | None = None
        for _ in range(attempts):
            try:
                return self.install(
                    key,
                    fetcher(timeout_seconds),
                    expected_bytes=expected_bytes,
                    sha256=sha256,
                    catalog_version=catalog_version,
                    supported_version=supported_version,
                    timeout_seconds=timeout_seconds,
                )
            except (OSError, TimeoutError, AssetCacheError) as error:
                last_error = error
                message = str(error)
                if any(token in message for token in ("incompatible", "storage exhausted", "invalid logical", "invalid immutable")):
                    break
        raise AssetCacheError(f"asset activation failed after {attempts} bounded attempts: {last_error}") from last_error

from __future__ import annotations

import hashlib
from pathlib import Path

import pytest

from app.asset_cache import AssetCacheError, VerifiedAssetCache


def feed(data: bytes, chunk: int = 7):
    for offset in range(0, len(data), chunk):
        yield data[offset:offset + chunk]


def install(cache, key: str, data: bytes, *, version: int = 4, supported: int = 4):
    return cache.install(key, feed(data), expected_bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), catalog_version=version, supported_version=supported)


def test_offline_restart_keeps_separate_global_and_clock_selections(tmp_path: Path):
    cache = VerifiedAssetCache(tmp_path, 100)
    install(cache, "global-a", b"global")
    install(cache, "clock-b", b"clock")
    cache.select("global", "global-a")
    cache.select("clock", "clock-b")
    restarted = VerifiedAssetCache(tmp_path, 100)
    assert restarted.selected("global").read_bytes() == b"global"
    assert restarted.selected("clock").read_bytes() == b"clock"


def test_unavailable_truncated_corrupt_and_incompatible_downloads_preserve_selection(tmp_path: Path):
    cache = VerifiedAssetCache(tmp_path, 100)
    install(cache, "wallpaper", b"valid")
    cache.select("global", "wallpaper")
    old = cache.selected("global").read_bytes()
    cases = [
        (lambda: (_ for _ in ()).throw(OSError("host unavailable")), 5, hashlib.sha256(b"valid").hexdigest(), 4, 4),
        (lambda: feed(b"bad"), 5, hashlib.sha256(b"valid").hexdigest(), 4, 4),
        (lambda: feed(b"other"), 5, hashlib.sha256(b"valid").hexdigest(), 4, 4),
        (lambda: feed(b"valid"), 5, hashlib.sha256(b"valid").hexdigest(), 5, 4),
    ]
    for chunks, size, digest, version, supported in cases:
        with pytest.raises((AssetCacheError, OSError)):
            cache.install("wallpaper", chunks(), expected_bytes=size, sha256=digest, catalog_version=version, supported_version=supported)
        assert cache.selected("global").read_bytes() == old


def test_eviction_skips_pinned_and_active_assets(tmp_path: Path):
    cache = VerifiedAssetCache(tmp_path, 8)
    install(cache, "pinned", b"1234")
    install(cache, "active", b"56")
    install(cache, "old", b"78")
    cache.select("global", "pinned")
    with cache.lease("active"):
        with pytest.raises(AssetCacheError, match="storage exhausted"):
            install(cache, "next", b"abcd")
        assert (tmp_path / cache.index["items"]["active"]["file"]).exists()
    install(cache, "next", b"abcd")
    assert cache.selected("global").read_bytes() == b"1234"
    assert "next" in cache.index["items"]
    assert sum(item["bytes"] for item in cache.index["items"].values()) <= 8


def test_storage_exhaustion_does_not_evict_selected_valid_asset(tmp_path: Path):
    cache = VerifiedAssetCache(tmp_path, 8)
    install(cache, "global", b"123456")
    cache.select("global", "global")
    with pytest.raises(AssetCacheError, match="storage exhausted"):
        install(cache, "other", b"123456")
    assert cache.selected("global").read_bytes() == b"123456"

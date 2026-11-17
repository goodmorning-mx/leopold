import base64
import hashlib
import json

import pytest
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from fastapi.testclient import TestClient
from sqlalchemy import create_engine, select
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.config import Settings, get_settings
from app.database import get_db
from app.main import app
from app.models import Base, Device, FirmwareInstallation

ADMIN = "firmware-admin-test-token"
IMAGE = b"ELF-test-app-image"


@pytest.fixture
def ctx(tmp_path):
    engine = create_engine("sqlite+pysqlite:///:memory:", connect_args={"check_same_thread": False}, poolclass=StaticPool)
    Base.metadata.create_all(engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)
    signer = ec.generate_private_key(ec.SECP256R1())
    public = signer.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo).decode()
    def override_db():
        with factory() as session:
            yield session
    def override_settings():
        return Settings(DATABASE_URL="sqlite+pysqlite:///:memory:",
                       LEOPOLD_ADMIN_PROVISIONING_TOKEN=ADMIN,
                       LEOPOLD_FIRMWARE_STORAGE_DIR=str(tmp_path / "private"),
                       LEOPOLD_FIRMWARE_SIGNING_PUBLIC_KEY_PEM=public)
    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_settings] = override_settings
    with TestClient(app) as client:
        yield client, factory, signer, tmp_path
    app.dependency_overrides.clear()
    Base.metadata.drop_all(engine)
    engine.dispose()


def manifest_for(release_id="rel-test-1", image=IMAGE, **changes):
    value = {"release_id": release_id, "source_commit": "a" * 40, "ui_revision": 18,
             "hardware_target": "esp32s3", "partition_layout": "leopold-ota-6m25-v1",
             "byte_length": len(image), "sha256": hashlib.sha256(image).hexdigest(),
             "signature_algorithm": "ECDSA-P256-SHA256", "key_id": "test-only", **changes}
    return value


def upload(client, signer, manifest, image=IMAGE, signature_override=None):
    public_der = signer.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    manifest["key_id"] = hashlib.sha256(public_der).hexdigest()[:16]
    canonical = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    signature = signature_override if signature_override is not None else signer.sign(canonical, ec.ECDSA(hashes.SHA256()))
    return client.put(f"/v1/admin/firmware/releases/{manifest['release_id']}", content=image,
                      headers={"Authorization": f"Bearer {ADMIN}",
                               "X-Leopold-Manifest": base64.b64encode(canonical).decode(),
                               "X-Leopold-Signature": base64.b64encode(signature).decode(),
                               "Content-Type": "application/octet-stream"})


def provision(client):
    response = client.post("/v1/admin/devices", headers={"Authorization": f"Bearer {ADMIN}"}, json={"label": "OTA test"})
    assert response.status_code == 201
    credential = response.json()
    return credential, {"X-Leopold-Device-ID": credential["device_id"], "Authorization": f"Bearer {credential['device_secret']}"}


def test_signed_release_assignment_private_download_revocation_and_idempotent_status(ctx):
    client, factory, signer, tmp_path = ctx
    manifest = manifest_for()
    assert upload(client, signer, manifest).status_code == 201
    credential, headers = provision(client)
    # Another device cannot discover or download this release.
    other, other_headers = provision(client)
    assert client.get("/v1/device/firmware/assignment", headers=other_headers).status_code == 204
    assert client.get("/v1/device/firmware/releases/rel-test-1/binary", headers=other_headers).status_code == 404

    admin = {"Authorization": f"Bearer {ADMIN}"}
    path = f"/v1/admin/devices/{credential['device_id']}/firmware-assignment"
    assert client.put(path, headers=admin, json={"release_id": "rel-test-1"}).status_code == 200
    assignment = client.get("/v1/device/firmware/assignment", headers=headers)
    assert assignment.status_code == 200 and assignment.json()["manifest"] == manifest
    binary_path = assignment.json()["binary_path"]
    assert client.get(binary_path, headers=headers).content == IMAGE

    status = {"event_id": "evt-1", "release_id": "rel-test-1", "state": "downloading", "detail": "started"}
    assert client.post("/v1/device/firmware/status", headers=headers, json=status).json() == {"ok": True, "duplicate": False}
    assert client.post("/v1/device/firmware/status", headers=headers, json=status).json() == {"ok": True, "duplicate": True}
    conflict = {**status, "state": "failed"}
    assert client.post("/v1/device/firmware/status", headers=headers, json=conflict).status_code == 409
    for terminal_state in ("healthy", "rolled_back"):
        terminal = {"event_id": f"terminal-{terminal_state}", "release_id": "rel-test-1",
                    "state": terminal_state}
        assert client.post("/v1/device/firmware/status", headers=headers, json=terminal).json() == {
            "ok": True, "duplicate": False}
        assert client.post("/v1/device/firmware/status", headers=headers, json=terminal).json() == {
            "ok": True, "duplicate": True}
    with factory() as db:
        assert len(db.scalars(select(FirmwareInstallation)).all()) == 3

    assert client.delete(path, headers=admin).status_code == 200
    assert client.get("/v1/device/firmware/assignment", headers=headers).status_code == 204
    assert client.get(binary_path, headers=headers).status_code == 404
    assert client.post("/v1/device/firmware/status", headers=headers, json={**status, "event_id": "evt-2", "state": "failed"}).status_code == 200
    saved = next((tmp_path / "private").glob("*.bin"))
    assert saved.read_bytes() == IMAGE and saved.stat().st_mode & 0o777 == 0o600


def test_local_ack_loss_retries_same_committed_healthy_event(ctx, monkeypatch):
    client, factory, signer, _tmp = ctx
    assert upload(client, signer, manifest_for()).status_code == 201
    credential, headers = provision(client)
    admin = {"Authorization": f"Bearer {ADMIN}"}
    path = f"/v1/admin/devices/{credential['device_id']}/firmware-assignment"
    assert client.put(path, headers=admin, json={"release_id": "rel-test-1"}).status_code == 200

    monkeypatch.setenv("APP_ENV", "local")
    monkeypatch.setenv("LEOPOLD_LOCAL_OTA_DROP_FIRST_HEALTHY_ACK", "1")
    app.state.local_ota_healthy_ack_failed = False
    status = {
        "event_id": "device-healthy-event-001",
        "release_id": "rel-test-1",
        "state": "healthy",
    }

    first = client.post("/v1/device/firmware/status", headers=headers, json=status)
    assert first.status_code == 503
    with factory() as db:
        rows = db.scalars(select(FirmwareInstallation)).all()
        assert len(rows) == 1
        assert (rows[0].event_id, rows[0].release_id, rows[0].state) == (
            status["event_id"], status["release_id"], "healthy"
        )

    retry = client.post("/v1/device/firmware/status", headers=headers, json=status)
    assert retry.status_code == 200
    assert retry.json() == {"ok": True, "duplicate": True}
    with factory() as db:
        assert len(db.scalars(select(FirmwareInstallation)).all()) == 1


@pytest.mark.parametrize("changes", [
    {"hardware_target": "esp32"},
    {"partition_layout": "wrong-layout"},
    {"byte_length": 6_553_601},
])
def test_rejects_incompatible_or_oversized_release(ctx, changes):
    client, _factory, signer, _tmp = ctx
    manifest = manifest_for(release_id="bad-release", **changes)
    response = upload(client, signer, manifest)
    assert response.status_code in (413, 422)


def test_rejects_bad_signature_and_hash(ctx):
    client, _factory, signer, _tmp = ctx
    manifest = manifest_for()
    assert upload(client, signer, manifest, signature_override=b"x" * 64).status_code == 422
    wrong = manifest_for(release_id="wrong-hash", sha256="0" * 64)
    assert upload(client, signer, wrong).status_code == 422
    truncated = manifest_for(release_id="truncated", image=IMAGE + b"extra")
    assert upload(client, signer, truncated, image=IMAGE).status_code == 422


def test_release_registration_requires_admin_and_manifest(ctx):
    client, _factory, signer, _tmp = ctx
    manifest = manifest_for()
    assert client.put("/v1/admin/firmware/releases/rel-test-1", content=IMAGE).status_code == 401
    response = client.put("/v1/admin/firmware/releases/rel-test-1", content=IMAGE,
                          headers={"Authorization": f"Bearer {ADMIN}"})
    assert response.status_code == 422

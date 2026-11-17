"""Private, device-authenticated firmware releases and assignments."""
import base64
import hashlib
import hmac
import json
import os
import re
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Annotated

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from fastapi import APIRouter, Depends, Header, HTTPException, Request, Response
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.auth_api import AuthenticatedUser
from app.config import Settings, get_settings
from app.database import get_db
from app.device_api import AuthenticatedDevice, require_admin_token
from app.models import (Device, FirmwareAssignment, FirmwareInstallation,
                        FirmwareRelease, User)

router = APIRouter(prefix="/v1")
Db = Annotated[Session, Depends(get_db)]
Config = Annotated[Settings, Depends(get_settings)]
MAX_APP_BYTES = 6_553_600
RELEASE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
STATES = {"assigned", "downloading", "verified", "rebooting", "healthy", "failed", "rolled_back"}
MANIFEST_FIELDS = {"release_id", "source_commit", "ui_revision", "hardware_target", "partition_layout", "byte_length", "sha256", "signature_algorithm", "key_id"}


class AssignmentBody(BaseModel):
    release_id: str = Field(min_length=1, max_length=64)


class StatusBody(BaseModel):
    event_id: str = Field(min_length=1, max_length=128)
    release_id: str = Field(min_length=1, max_length=64)
    state: str
    detail: str | None = Field(default=None, max_length=256)


def _public_key(settings: Settings) -> ec.EllipticCurvePublicKey:
    if not settings.firmware_signing_public_key_pem:
        raise HTTPException(503, "Firmware release signing is not configured")
    try:
        key = serialization.load_pem_public_key(settings.firmware_signing_public_key_pem.encode())
    except (ValueError, TypeError) as exc:
        raise HTTPException(503, "Firmware signing key configuration is invalid") from exc
    if not isinstance(key, ec.EllipticCurvePublicKey) or not isinstance(key.curve, ec.SECP256R1):
        raise HTTPException(503, "Firmware signing key must be ECDSA P-256")
    return key


def _assigned_release(db: Session, device: Device) -> FirmwareRelease | None:
    assignment = db.get(FirmwareAssignment, device.id)
    if assignment is None or assignment.revoked or assignment.release_id is None:
        return None
    return db.get(FirmwareRelease, assignment.release_id)


@router.put("/admin/firmware/releases/{release_id}", status_code=201)
async def register_release(release_id: str, request: Request, db: Db, settings: Config,
                           authorization: Annotated[str | None, Header()] = None,
                           x_leopold_manifest: Annotated[str | None, Header()] = None,
                           x_leopold_signature: Annotated[str | None, Header()] = None):
    """Register exact application bytes; signed canonical manifest is in headers."""
    require_admin_token(settings, authorization)
    if not RELEASE_ID.fullmatch(release_id):
        raise HTTPException(422, "Invalid release ID")
    if db.get(FirmwareRelease, release_id):
        raise HTTPException(409, "Release IDs are immutable")
    if not x_leopold_manifest or not x_leopold_signature:
        raise HTTPException(422, "Signed release manifest required")
    try:
        manifest_bytes = base64.b64decode(x_leopold_manifest, validate=True)
        manifest = json.loads(manifest_bytes)
        signature = base64.b64decode(x_leopold_signature, validate=True)
    except (ValueError, json.JSONDecodeError) as exc:
        raise HTTPException(422, "Malformed signed manifest") from exc
    canonical = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    if canonical != manifest_bytes or set(manifest) != MANIFEST_FIELDS:
        raise HTTPException(422, "Manifest is not canonical or has unexpected fields")
    try:
        public_key = _public_key(settings)
        public_der = public_key.public_bytes(serialization.Encoding.DER,
                                             serialization.PublicFormat.SubjectPublicKeyInfo)
        key_id = hashlib.sha256(public_der).hexdigest()[:16]
        if manifest.get("key_id") != key_id:
            raise HTTPException(422, "Manifest key ID does not match configured signer")
        public_key.verify(signature, canonical, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature as exc:
        raise HTTPException(422, "Invalid release signature") from exc
    except HTTPException:
        raise
    if manifest.get("signature_algorithm") != "ECDSA-P256-SHA256" or manifest.get("release_id") != release_id:
        raise HTTPException(422, "Release signature metadata mismatch")
    if not re.fullmatch(r"[0-9a-f]{40}", str(manifest.get("source_commit", ""))):
        raise HTTPException(422, "Full source commit required")
    if not isinstance(manifest.get("ui_revision"), int) or manifest["ui_revision"] < 1:
        raise HTTPException(422, "Invalid UI revision")
    if manifest.get("hardware_target") != "esp32s3" or manifest.get("partition_layout") != "leopold-ota-6m25-v1":
        raise HTTPException(422, "Unsupported hardware or partition layout")
    length = manifest.get("byte_length")
    if not isinstance(length, int) or length < 1 or length > MAX_APP_BYTES:
        raise HTTPException(413, "Application exceeds OTA slot limit")
    digest = manifest.get("sha256")
    if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise HTTPException(422, "Invalid SHA-256")
    chunks: list[bytes] = []
    received = 0
    async for chunk in request.stream():
        received += len(chunk)
        if received > MAX_APP_BYTES:
            raise HTTPException(413, "Application exceeds OTA slot limit")
        chunks.append(chunk)
    data = b"".join(chunks)
    if len(data) != length:
        raise HTTPException(422, "Application length does not match manifest")
    if not hmac.compare_digest(hashlib.sha256(data).hexdigest(), digest):
        raise HTTPException(422, "Application digest does not match manifest")
    root = Path(settings.firmware_storage_dir).resolve()
    root.mkdir(mode=0o700, parents=True, exist_ok=True)
    target = root / f"{release_id}.bin"
    if target.parent != root or target.exists():
        raise HTTPException(409, "Release artifact already exists")
    fd, tmp = tempfile.mkstemp(prefix=".upload-", dir=root)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "wb") as out:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        os.replace(tmp, target)
        db.add(FirmwareRelease(id=release_id, manifest=manifest,
                               signature=base64.b64encode(signature).decode(),
                               artifact_path=str(target)))
        db.commit()
    except Exception:
        db.rollback()
        Path(tmp).unlink(missing_ok=True)
        target.unlink(missing_ok=True)
        raise
    return {"release_id": release_id, "manifest": manifest}


@router.put("/admin/devices/{device_id}/firmware-assignment")
def assign_release(device_id: str, payload: AssignmentBody, db: Db, settings: Config,
                   authorization: Annotated[str | None, Header()] = None):
    require_admin_token(settings, authorization)
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    release = db.get(FirmwareRelease, payload.release_id)
    if device is None or release is None:
        raise HTTPException(404, "Device or release not found")
    assignment = db.get(FirmwareAssignment, device.id)
    if assignment is None:
        assignment = FirmwareAssignment(device_id=device.id, release_id=release.id, revoked=False)
        db.add(assignment)
    else:
        assignment.release_id, assignment.revoked = release.id, False
    db.commit()
    return {"device_id": device_id, "release_id": release.id, "revoked": False}


@router.delete("/admin/devices/{device_id}/firmware-assignment")
def revoke_release(device_id: str, db: Db, settings: Config,
                   authorization: Annotated[str | None, Header()] = None):
    require_admin_token(settings, authorization)
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    if device is None:
        raise HTTPException(404, "Device not found")
    assignment = db.get(FirmwareAssignment, device.id)
    if assignment is not None:
        assignment.revoked = True
        db.commit()
    return {"device_id": device_id, "revoked": True}


@router.put("/me/devices/{device_id}/firmware-assignment")
def owner_assign_release(device_id: str, payload: AssignmentBody, user: AuthenticatedUser, db: Db):
    device = db.scalar(select(Device).where(Device.device_id == device_id, Device.owner_id == user.id))
    release = db.get(FirmwareRelease, payload.release_id)
    if device is None or release is None:
        raise HTTPException(404, "Device or release not found")
    assignment = db.get(FirmwareAssignment, device.id)
    if assignment is None:
        assignment = FirmwareAssignment(device_id=device.id, release_id=release.id, revoked=False)
        db.add(assignment)
    else:
        assignment.release_id, assignment.revoked = release.id, False
    db.commit()
    return {"device_id": device_id, "release_id": release.id, "revoked": False}


@router.delete("/me/devices/{device_id}/firmware-assignment")
def owner_revoke_release(device_id: str, user: AuthenticatedUser, db: Db):
    device = db.scalar(select(Device).where(Device.device_id == device_id, Device.owner_id == user.id))
    if device is None:
        raise HTTPException(404, "Device not found")
    assignment = db.get(FirmwareAssignment, device.id)
    if assignment is not None:
        assignment.revoked = True
        db.commit()
    return {"device_id": device_id, "revoked": True}


@router.get("/admin/devices/{device_id}/firmware")
def inspect_device_firmware(device_id: str, db: Db, settings: Config,
                            authorization: Annotated[str | None, Header()] = None):
    require_admin_token(settings, authorization)
    device = db.scalar(select(Device).where(Device.device_id == device_id))
    if device is None:
        raise HTTPException(404, "Device not found")
    assignment = db.get(FirmwareAssignment, device.id)
    history = db.scalars(select(FirmwareInstallation).where(
        FirmwareInstallation.device_id == device.id).order_by(FirmwareInstallation.created_at.desc())).all()
    return {"device_id": device_id,
            "assignment": None if assignment is None else {"release_id": assignment.release_id, "revoked": assignment.revoked},
            "history": [{"event_id": row.event_id, "release_id": row.release_id,
                         "state": row.state, "detail": row.detail, "created_at": row.created_at}
                        for row in history]}


@router.get("/device/firmware/assignment")
def get_assignment(device: AuthenticatedDevice, db: Db):
    release = _assigned_release(db, device)
    if release is None:
        return Response(status_code=204)
    return {"release_id": release.id, "manifest": release.manifest,
            "signature": release.signature,
            "binary_path": f"/v1/device/firmware/releases/{release.id}/binary"}


@router.get("/device/firmware/releases/{release_id}/binary")
def download_release(release_id: str, device: AuthenticatedDevice, db: Db):
    release = _assigned_release(db, device)
    if release is None or release.id != release_id:
        raise HTTPException(404, "Firmware release not found")
    return FileResponse(release.artifact_path, media_type="application/octet-stream",
                        filename=f"{release_id}.bin",
                        headers={"Cache-Control": "private, no-store", "X-Content-Type-Options": "nosniff"})


@router.post("/device/firmware/status")
def report_status(payload: StatusBody, device: AuthenticatedDevice, db: Db):
    if payload.state not in STATES:
        raise HTTPException(422, "Invalid firmware state")
    release = _assigned_release(db, device)
    if release is None or release.id != payload.release_id:
        # Permit reporting a prior in-progress release after revocation, but only if it was assigned to this device.
        assignment = db.get(FirmwareAssignment, device.id)
        previous = db.scalar(select(FirmwareInstallation).where(
            FirmwareInstallation.device_id == device.id,
            FirmwareInstallation.release_id == payload.release_id))
        if (assignment is None or assignment.release_id != payload.release_id) and previous is None:
            raise HTTPException(403, "Release is not authorized for this device")
    existing = db.scalar(select(FirmwareInstallation).where(
        FirmwareInstallation.device_id == device.id,
        FirmwareInstallation.event_id == payload.event_id))
    if existing:
        if (existing.release_id, existing.state, existing.detail) != (payload.release_id, payload.state, payload.detail):
            raise HTTPException(409, "Status event ID was already used")
        return {"ok": True, "duplicate": True}
    db.add(FirmwareInstallation(device_id=device.id, event_id=payload.event_id,
                                release_id=payload.release_id, state=payload.state,
                                detail=payload.detail, created_at=datetime.now(timezone.utc)))
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        raise HTTPException(409, "Status event conflict")
    return {"ok": True, "duplicate": False}


@router.get("/me/devices/{device_id}/firmware")
def owner_firmware(device_id: str, user: AuthenticatedUser, db: Db):
    device = db.scalar(select(Device).where(Device.device_id == device_id, Device.owner_id == user.id))
    if device is None:
        raise HTTPException(404, "Device not found")
    assignment = db.get(FirmwareAssignment, device.id)
    history = db.scalars(select(FirmwareInstallation).where(
        FirmwareInstallation.device_id == device.id).order_by(FirmwareInstallation.created_at.desc())).all()
    return {"device_id": device_id,
            "assignment": None if assignment is None else {"release_id": assignment.release_id, "revoked": assignment.revoked},
            "history": [{"release_id": row.release_id, "state": row.state, "detail": row.detail,
                         "created_at": row.created_at} for row in history]}

import hashlib
import hmac
import secrets
from datetime import datetime, timedelta, timezone
from typing import Annotated

from fastapi import APIRouter, Depends, Header, HTTPException, Request, Response, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError, OperationalError
from sqlalchemy.orm import Session

from app.config import Settings, get_settings
from app.computer_controls import CONTROL_COMPONENT_IDS, with_computer_controls
from app.database import get_db
from app.models import (
    Device,
    DeviceClaimToken,
    DeviceEvent,
    DeviceStatus,
    DeviceUIDocument,
    SpotifyConnection,
    YouTubeConnection,
)
from app.schemas import (
    ConnectRequest,
    ConnectResponse,
    ComputerCommand,
    DeviceEventRequest,
    HeartbeatRequest,
    HeartbeatResponse,
    OnboardingStatusResponse,
    ProvisionDeviceRequest,
    ProvisionDeviceResponse,
    Screen,
    UIDocument,
    UIPublishDocument,
)

router = APIRouter(prefix="/v1")
Db = Annotated[Session, Depends(get_db)]
Config = Annotated[Settings, Depends(get_settings)]
_DUMMY_HASH = hashlib.sha256(b"invalid-device-secret").hexdigest()
CLAIM_MINUTES = 15


def secret_hash(secret: str) -> str:
    return hashlib.sha256(secret.encode("utf-8")).hexdigest()


def onboarding_url(
    db: Session,
    device: Device,
    settings: Settings,
    requested_token: str | None = None,
) -> str | None:
    if device.owner_id is not None and requested_token is None:
        return None
    now = datetime.now(timezone.utc)
    raw_token = requested_token or secrets.token_urlsafe(24)
    token_digest = secret_hash(raw_token)
    existing = db.scalar(
        select(DeviceClaimToken).where(
            DeviceClaimToken.token_hash == token_digest
        )
    )
    if existing is not None:
        if existing.device_id != device.id:
            raise HTTPException(status_code=409, detail="Setup session conflict")
        existing.expires_at = now + timedelta(minutes=CLAIM_MINUTES)
        if device.owner_id is not None:
            existing.used_at = now
            return None
        if existing.used_at is not None:
            raise HTTPException(status_code=409, detail="Setup session conflict")
        web_base = settings.allowed_origins[0].rstrip("/")
        return f"{web_base}/setup/{raw_token}"

    if device.owner_id is not None:
        db.add(
            DeviceClaimToken(
                device_id=device.id,
                token_hash=token_digest,
                expires_at=now + timedelta(minutes=CLAIM_MINUTES),
                used_at=now,
            )
        )
        return None

    db.query(DeviceClaimToken).filter(
        DeviceClaimToken.device_id == device.id,
        DeviceClaimToken.used_at.is_(None),
    ).update({DeviceClaimToken.used_at: now})
    db.add(
        DeviceClaimToken(
            device_id=device.id,
            token_hash=token_digest,
            expires_at=now + timedelta(minutes=CLAIM_MINUTES),
        )
    )
    web_base = settings.allowed_origins[0].rstrip("/")
    return f"{web_base}/setup/{raw_token}"


def require_admin_token(settings: Settings, authorization: str | None) -> None:
    configured_token = settings.admin_provisioning_token
    if not configured_token:
        raise HTTPException(status_code=503, detail="Provisioning is disabled")
    supplied_token = (
        authorization[7:]
        if authorization and authorization.startswith("Bearer ")
        else ""
    )
    if not supplied_token or not hmac.compare_digest(supplied_token, configured_token):
        raise HTTPException(
            status_code=401,
            detail="Invalid provisioning credentials",
            headers={"WWW-Authenticate": "Bearer"},
        )


def active_ui(db: Session, device: Device) -> DeviceUIDocument:
    document = db.scalar(
        select(DeviceUIDocument).where(
            DeviceUIDocument.device_id == device.id,
            DeviceUIDocument.is_active.is_(True),
        )
    )
    if document is None:
        raise HTTPException(status_code=503, detail="Device UI is unavailable")
    return document


def initial_document(revision: int, poll_seconds: int) -> dict:
    return UIDocument(
        schema_version=1,
        revision=revision,
        poll_after_ms=poll_seconds * 1000,
        screens=[{
            "id": "home",
            "background": "#000000",
            "components": [
                {
                    "type": "text",
                    "id": "greeting",
                    "text": "Buenos días",
                    "x": 233,
                    "y": 105,
                    "align": "center",
                    "size": 3,
                    "color": "#FFFFFF",
                },
                {
                    "type": "indicator",
                    "id": "cloud",
                    "label": "GoodMorning Cloud",
                    "state": "connected",
                    "x": 233,
                    "y": 230,
                },
                {
                    "type": "button",
                    "id": "continue",
                    "label": "Continuar",
                    "x": 133,
                    "y": 315,
                    "width": 200,
                    "height": 58,
                },
                {
                    "type": "button",
                    "id": "open-usb",
                    "label": "USB",
                    "icon": "usb",
                    "x": 40,
                    "y": 315,
                    "width": 72,
                    "height": 72,
                    "color": "#3D7FFF",
                },
            ],
        }],
    ).model_dump(mode="json")


def authenticate_device(
    db: Db,
    device_id: Annotated[str | None, Header(alias="X-Leopold-Device-ID")] = None,
    authorization: Annotated[str | None, Header()] = None,
) -> Device:
    supplied_secret = ""
    if authorization and authorization.startswith("Bearer "):
        supplied_secret = authorization[7:]
    device = db.scalar(select(Device).where(Device.device_id == (device_id or "")))
    expected = device.secret_hash if device is not None else _DUMMY_HASH
    authenticated = bool(supplied_secret) and hmac.compare_digest(
        secret_hash(supplied_secret), expected
    )
    if not authenticated or device is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid device credentials",
            headers={"WWW-Authenticate": "Bearer"},
        )
    if device.status == DeviceStatus.disabled:
        raise HTTPException(status_code=403, detail="Device is disabled")
    return device


AuthenticatedDevice = Annotated[Device, Depends(authenticate_device)]


@router.post(
    "/admin/devices",
    response_model=ProvisionDeviceResponse,
    status_code=status.HTTP_201_CREATED,
)
def provision_device(
    payload: ProvisionDeviceRequest,
    db: Db,
    settings: Config,
    authorization: Annotated[str | None, Header()] = None,
) -> ProvisionDeviceResponse:
    require_admin_token(settings, authorization)

    for _ in range(5):
        device_secret = secrets.token_urlsafe(32)
        device = Device(
            device_id=f"LPD-{secrets.token_hex(3).upper()}",
            label=payload.label,
            secret_hash=secret_hash(device_secret),
            status=DeviceStatus.provisioned,
            capabilities={},
        )
        device.ui_documents.append(
            DeviceUIDocument(
                revision=1,
                document=initial_document(1, settings.ui_poll_interval_seconds),
                is_active=True,
            )
        )
        db.add(device)
        try:
            db.commit()
            return ProvisionDeviceResponse(
                device_id=device.device_id, device_secret=device_secret
            )
        except IntegrityError:
            db.rollback()
    raise HTTPException(status_code=503, detail="Could not allocate a device ID")


@router.put("/admin/devices/{device_id}/ui", response_model=UIDocument)
def publish_device_ui(
    device_id: str,
    payload: UIPublishDocument | Screen,
    db: Db,
    settings: Config,
    authorization: Annotated[str | None, Header()] = None,
) -> Response:
    """Atomically replace the active SDUI document with its next revision."""

    require_admin_token(settings, authorization)
    try:
        device = db.scalar(
            select(Device)
            .where(Device.device_id == device_id)
            .with_for_update(nowait=True)
        )
        if device is None:
            raise HTTPException(status_code=404, detail="Device not found")

        previous = db.scalar(
            select(DeviceUIDocument)
            .where(
                DeviceUIDocument.device_id == device.id,
                DeviceUIDocument.is_active.is_(True),
            )
            .with_for_update(nowait=True)
        )
        if previous is None:
            raise HTTPException(status_code=409, detail="Active UI revision conflict")

        revision = previous.revision + 1
        poll_after_ms = (
            payload.poll_after_ms
            if isinstance(payload, UIPublishDocument)
            else settings.ui_poll_interval_seconds * 1000
        )
        document = UIDocument(
            schema_version=1,
            revision=revision,
            poll_after_ms=poll_after_ms,
            screens=payload.screens
            if isinstance(payload, UIPublishDocument)
            else [payload],
        )

        previous.is_active = False
        db.flush()
        db.add(
            DeviceUIDocument(
                device_id=device.id,
                revision=revision,
                document=document.model_dump(mode="json"),
                is_active=True,
            )
        )
        db.commit()
    except (IntegrityError, OperationalError):
        db.rollback()
        raise HTTPException(status_code=409, detail="UI revision conflict") from None

    etag = f'"{revision}"'
    return Response(
        content=document.model_dump_json(),
        media_type="application/json",
        headers={"ETag": etag, "Cache-Control": "private, no-cache"},
    )


def mark_seen(device: Device, request: Request) -> None:
    device.last_seen_at = datetime.now(timezone.utc)
    device.last_ip = request.client.host if request.client else None
    device.status = DeviceStatus.online


@router.post("/device/connect", response_model=ConnectResponse)
def connect_device(
    payload: ConnectRequest,
    request: Request,
    db: Db,
    settings: Config,
    device: AuthenticatedDevice,
) -> ConnectResponse:
    device.firmware_version = payload.firmware_version
    device.capabilities = payload.capabilities
    mark_seen(device, request)
    revision = active_ui(db, device).revision
    setup_url = onboarding_url(db, device, settings, payload.setup_token)
    claimed = device.owner_id is not None
    db.commit()
    return ConnectResponse(
        device_id=device.device_id,
        server_time=datetime.now(timezone.utc),
        heartbeat_interval_seconds=settings.heartbeat_interval_seconds,
        ui_poll_interval_seconds=settings.ui_poll_interval_seconds,
        ui_revision=revision,
        claimed=claimed,
        owner_display_name=device.owner.display_name if device.owner else None,
        onboarding_url=setup_url,
    )


@router.post(
    "/device/onboarding/status",
    response_model=OnboardingStatusResponse,
)
def device_onboarding_status(
    db: Db,
    device: AuthenticatedDevice,
) -> OnboardingStatusResponse:
    db.refresh(device)
    return OnboardingStatusResponse(
        claimed=device.owner_id is not None,
        owner_display_name=device.owner.display_name if device.owner else None,
    )


@router.get("/device/ui", response_model=UIDocument)
def get_device_ui(
    db: Db,
    settings: Config,
    device: AuthenticatedDevice,
    if_none_match: Annotated[str | None, Header(alias="If-None-Match")] = None,
) -> Response:
    stored = active_ui(db, device)
    etag = f'"controls-v1-{stored.revision}"'
    spotify_connection = (
        db.scalar(
            select(SpotifyConnection).where(
                SpotifyConnection.user_id == device.owner_id,
                SpotifyConnection.active.is_(True),
            )
        )
        if device.owner_id
        else None
    )
    youtube_connection = (
        db.scalar(
            select(YouTubeConnection).where(
                YouTubeConnection.user_id == device.owner_id,
                YouTubeConnection.active.is_(True),
            )
        )
        if device.owner_id
        else None
    )
    if spotify_connection is not None or youtube_connection is not None:
        from app.spotify import player_state, spotify_document
        from app.youtube import youtube_document, youtube_screen

        revision = int(datetime.now(timezone.utc).timestamp())
        owner_name = device.owner.display_name if device.owner else "Leopold"
        if spotify_connection is not None:
            state = player_state(db, spotify_connection, settings)
            state["owner_name"] = owner_name
            state["bluetooth_name"] = f"Leopold-{device.device_id[-4:]}"
            raw_document = spotify_document(revision, state)
            if youtube_connection is not None:
                youtube = youtube_screen(
                    youtube_connection.cached_feed or [],
                    youtube_connection.display_name,
                )
                bluetooth_index = next(
                    (
                        index
                        for index, candidate in enumerate(raw_document["screens"])
                        if candidate["id"] == "bluetooth"
                    ),
                    len(raw_document["screens"]),
                )
                raw_document["screens"].insert(bluetooth_index, youtube)
        else:
            raw_document = youtube_document(
                revision,
                youtube_connection.cached_feed or [],
                youtube_connection.display_name,
                owner_name,
            )
        live_document = UIDocument.model_validate(with_computer_controls(raw_document))
        services = "spotify-youtube" if spotify_connection and youtube_connection else (
            "spotify" if spotify_connection else "youtube"
        )
        live_etag = f'"{services}-{live_document.revision}"'
        return Response(
            content=live_document.model_dump_json(),
            media_type="application/json",
            headers={"ETag": live_etag, "Cache-Control": "private, no-store"},
        )
    if if_none_match and any(
        tag.strip() in {etag, f"W/{etag}"} for tag in if_none_match.split(",")
    ):
        return Response(status_code=304, headers={"ETag": etag})
    document = UIDocument.model_validate(with_computer_controls(stored.document))
    return Response(
        content=document.model_dump_json(),
        media_type="application/json",
        headers={"ETag": etag, "Cache-Control": "private, no-cache"},
    )


@router.post("/device/events", status_code=status.HTTP_202_ACCEPTED)
def record_event(
    payload: DeviceEventRequest,
    db: Db,
    settings: Config,
    device: AuthenticatedDevice,
) -> dict[str, bool]:
    from app.spotify import handle_device_spotify_event
    from app.youtube import handle_device_youtube_event

    handle_device_spotify_event(db, device, payload.component_id, settings)
    handle_device_youtube_event(db, device, payload.component_id, settings)
    existing = db.scalar(
        select(DeviceEvent.id).where(
            DeviceEvent.device_id == device.id,
            DeviceEvent.event_id == payload.event_id,
        )
    )
    if existing is None:
        db.add(
            DeviceEvent(
                device_id=device.id,
                event_id=payload.event_id,
                revision=payload.revision,
                type=payload.type,
                component_id=payload.component_id,
                payload=payload.payload,
                occurred_at=payload.occurred_at,
            )
        )
        try:
            db.commit()
        except IntegrityError:
            db.rollback()
    return {"accepted": True}


@router.get("/device/computer-commands", response_model=list[ComputerCommand])
def computer_commands(
    after: datetime,
    db: Db,
    device: AuthenticatedDevice,
) -> list[dict]:
    """Return this device's recent allowlisted Mac commands for a local companion."""
    events = db.scalars(
            select(DeviceEvent)
            .where(
                DeviceEvent.device_id == device.id,
                DeviceEvent.component_id.in_(CONTROL_COMPONENT_IDS),
                DeviceEvent.received_at > after,
            )
            .order_by(DeviceEvent.received_at, DeviceEvent.id)
            .limit(50)
        ).all()
    return [
        {
            "event_id": event.event_id,
            "component_id": event.component_id,
            "received_at": event.received_at,
        }
        for event in events
    ]


@router.post("/device/heartbeat", response_model=HeartbeatResponse)
def heartbeat(
    payload: HeartbeatRequest,
    request: Request,
    db: Db,
    device: AuthenticatedDevice,
) -> HeartbeatResponse:
    device.firmware_version = payload.firmware_version
    device.wifi_rssi = payload.wifi_rssi
    device.battery_percent = payload.battery_percent
    device.uptime_seconds = payload.uptime_seconds
    mark_seen(device, request)
    revision = active_ui(db, device).revision
    db.commit()
    return HeartbeatResponse(
        ok=True, server_time=datetime.now(timezone.utc), ui_revision=revision
    )

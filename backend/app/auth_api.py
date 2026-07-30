import hashlib
import hmac
import re
import secrets
from datetime import datetime, timedelta, timezone
from typing import Annotated

from fastapi import APIRouter, Cookie, Depends, HTTPException, Response, status
from fastapi.responses import RedirectResponse
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from app.config import Settings, get_settings
from app.database import get_db
from app.models import Device, DeviceClaimToken, User, UserSession
from app.schemas import (
    ClaimedDeviceResponse,
    ClaimStatusResponse,
    LoginRequest,
    RegisterRequest,
    UserResponse,
)

router = APIRouter(prefix="/v1")
Db = Annotated[Session, Depends(get_db)]
Config = Annotated[Settings, Depends(get_settings)]

SESSION_COOKIE = "leopold_session"
SESSION_DAYS = 30
EMAIL_PATTERN = re.compile(r"^[^@\s]+@[^@\s]+\.[^@\s]+$")


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def as_utc(value: datetime) -> datetime:
    return value if value.tzinfo is not None else value.replace(tzinfo=timezone.utc)


def password_hash(password: str, salt: bytes | None = None) -> str:
    actual_salt = salt or secrets.token_bytes(16)
    derived = hashlib.scrypt(
        password.encode("utf-8"),
        salt=actual_salt,
        n=2**14,
        r=8,
        p=1,
        dklen=32,
    )
    return f"scrypt${actual_salt.hex()}${derived.hex()}"


def password_matches(password: str, encoded: str) -> bool:
    try:
        algorithm, salt_hex, expected_hex = encoded.split("$", 2)
        if algorithm != "scrypt":
            return False
        actual = password_hash(password, bytes.fromhex(salt_hex)).split("$", 2)[2]
        return hmac.compare_digest(actual, expected_hex)
    except (ValueError, TypeError):
        return False


def normalize_email(email: str) -> str:
    normalized = email.strip().lower()
    if not EMAIL_PATTERN.fullmatch(normalized):
        raise HTTPException(status_code=422, detail="Enter a valid email address")
    return normalized


def set_session(response: Response, db: Session, user: User, settings: Settings) -> None:
    raw_token = secrets.token_urlsafe(32)
    expires_at = datetime.now(timezone.utc) + timedelta(days=SESSION_DAYS)
    db.add(
        UserSession(
            user_id=user.id,
            token_hash=token_hash(raw_token),
            expires_at=expires_at,
        )
    )
    db.commit()
    response.set_cookie(
        SESSION_COOKIE,
        raw_token,
        max_age=SESSION_DAYS * 24 * 60 * 60,
        httponly=True,
        secure=settings.app_env != "local",
        samesite="lax",
        path="/",
    )


def current_user(
    db: Db,
    session_token: Annotated[str | None, Cookie(alias=SESSION_COOKIE)] = None,
) -> User:
    if not session_token:
        raise HTTPException(status_code=401, detail="Authentication required")
    now = datetime.now(timezone.utc)
    session = db.scalar(
        select(UserSession).where(
            UserSession.token_hash == token_hash(session_token),
            UserSession.expires_at > now,
        )
    )
    if session is None:
        raise HTTPException(status_code=401, detail="Authentication required")
    return session.user


AuthenticatedUser = Annotated[User, Depends(current_user)]


@router.post(
    "/auth/register", response_model=UserResponse, status_code=status.HTTP_201_CREATED
)
def register(
    payload: RegisterRequest,
    response: Response,
    db: Db,
    settings: Config,
) -> UserResponse:
    email = normalize_email(payload.email)
    display_name = payload.display_name.strip()
    if not display_name:
        raise HTTPException(status_code=422, detail="Display name is required")
    user = User(
        email=email,
        display_name=display_name,
        password_hash=password_hash(payload.password),
    )
    db.add(user)
    try:
        db.flush()
    except IntegrityError:
        db.rollback()
        raise HTTPException(status_code=409, detail="Email is already registered")
    set_session(response, db, user, settings)
    return UserResponse(email=user.email, display_name=user.display_name)


@router.post("/auth/login", response_model=UserResponse)
def login(
    payload: LoginRequest,
    response: Response,
    db: Db,
    settings: Config,
) -> UserResponse:
    email = normalize_email(payload.email)
    user = db.scalar(select(User).where(User.email == email))
    valid = user is not None and password_matches(payload.password, user.password_hash)
    if not valid or user is None:
        raise HTTPException(status_code=401, detail="Invalid email or password")
    set_session(response, db, user, settings)
    return UserResponse(email=user.email, display_name=user.display_name)


@router.post("/auth/logout", status_code=status.HTTP_204_NO_CONTENT)
def logout(
    response: Response,
    db: Db,
    session_token: Annotated[str | None, Cookie(alias=SESSION_COOKIE)] = None,
) -> Response:
    if session_token:
        session = db.scalar(
            select(UserSession).where(
                UserSession.token_hash == token_hash(session_token)
            )
        )
        if session is not None:
            db.delete(session)
            db.commit()
    response.delete_cookie(SESSION_COOKIE, path="/")
    response.status_code = status.HTTP_204_NO_CONTENT
    return response


@router.get("/auth/me", response_model=UserResponse)
def me(user: AuthenticatedUser) -> UserResponse:
    return UserResponse(email=user.email, display_name=user.display_name)


def find_claim(db: Session, token: str) -> DeviceClaimToken | None:
    return db.scalar(
        select(DeviceClaimToken).where(
            DeviceClaimToken.token_hash == token_hash(token)
        )
    )


@router.get("/onboarding/{token}", response_model=ClaimStatusResponse)
def onboarding_status(
    token: str,
    response: Response,
    db: Db,
) -> ClaimStatusResponse:
    response.headers["Cache-Control"] = "no-store, max-age=0"
    response.headers["Pragma"] = "no-cache"
    claim = find_claim(db, token)
    now = datetime.now(timezone.utc)
    if claim is None:
        return ClaimStatusResponse(valid=False, claimed=False, pending=True)
    expired = as_utc(claim.expires_at) <= now
    return ClaimStatusResponse(
        valid=not expired and claim.used_at is None,
        claimed=claim.device.owner_id is not None,
        expires_at=claim.expires_at,
    )


@router.get("/onboarding/{token}/continue", include_in_schema=False)
def continue_onboarding(
    token: str,
    settings: Config,
    ssid: str | None = None,
) -> RedirectResponse:
    if not re.fullmatch(r"[A-Fa-f0-9]{32,64}", token):
        raise HTTPException(status_code=404, detail="Setup session not found")
    if ssid is not None and not re.fullmatch(r"Leopold-[A-Za-z0-9-]{1,16}", ssid):
        raise HTTPException(status_code=422, detail="Invalid setup network")
    web_base = settings.allowed_origins[0].rstrip("/")
    destination = f"{web_base}/setup/{token}"
    if ssid:
        destination += f"?ssid={ssid}"
    return RedirectResponse(
        destination,
        status_code=307,
        headers={"Cache-Control": "no-store, max-age=0"},
    )


@router.post("/onboarding/{token}/claim", response_model=ClaimStatusResponse)
def claim_device(
    token: str,
    db: Db,
    user: AuthenticatedUser,
) -> ClaimStatusResponse:
    now = datetime.now(timezone.utc)
    claim = db.scalar(
        select(DeviceClaimToken)
        .where(DeviceClaimToken.token_hash == token_hash(token))
        .with_for_update()
    )
    if claim is None or as_utc(claim.expires_at) <= now or claim.used_at is not None:
        raise HTTPException(status_code=410, detail="Setup link is expired")
    device = db.scalar(select(Device).where(Device.id == claim.device_id).with_for_update())
    if device is None:
        raise HTTPException(status_code=404, detail="Device not found")
    if device.owner_id is not None and device.owner_id != user.id:
        raise HTTPException(status_code=409, detail="Device is already registered")
    device.owner_id = user.id
    claim.used_at = now
    db.commit()
    return ClaimStatusResponse(valid=False, claimed=True, expires_at=claim.expires_at)


@router.get("/me/devices", response_model=list[ClaimedDeviceResponse])
def my_devices(db: Db, user: AuthenticatedUser) -> list[ClaimedDeviceResponse]:
    devices = db.scalars(
        select(Device).where(Device.owner_id == user.id).order_by(Device.created_at)
    ).all()
    return [
        ClaimedDeviceResponse(
            device_id=device.device_id,
            label=device.label,
            status=device.status.value,
            firmware_version=device.firmware_version,
            last_seen_at=device.last_seen_at,
        )
        for device in devices
    ]


@router.delete("/me/devices/{device_id}", status_code=status.HTTP_204_NO_CONTENT)
def remove_my_device(
    device_id: str,
    db: Db,
    user: AuthenticatedUser,
) -> Response:
    device = db.scalar(
        select(Device)
        .where(Device.device_id == device_id, Device.owner_id == user.id)
        .with_for_update()
    )
    if device is None:
        raise HTTPException(status_code=404, detail="Device not found")
    now = datetime.now(timezone.utc)
    db.query(DeviceClaimToken).filter(
        DeviceClaimToken.device_id == device.id,
        DeviceClaimToken.used_at.is_(None),
    ).update({DeviceClaimToken.used_at: now})
    device.owner_id = None
    db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)

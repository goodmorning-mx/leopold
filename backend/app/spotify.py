import base64
from datetime import datetime, timedelta, timezone

import httpx
from cryptography.fernet import Fernet, InvalidToken
from fastapi import HTTPException
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.config import Settings
from app.models import Device, DeviceUIDocument, SpotifyConnection, User

SPOTIFY_API = "https://api.spotify.com/v1"
SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token"
SPOTIFY_SCOPES = " ".join(
    (
        "user-read-private",
        "user-read-playback-state",
        "user-read-currently-playing",
        "user-modify-playback-state",
    )
)


def utc(value: datetime) -> datetime:
    return value if value.tzinfo else value.replace(tzinfo=timezone.utc)


def require_spotify_config(settings: Settings) -> None:
    if not (
        settings.spotify_client_id
        and settings.spotify_client_secret
        and settings.integration_encryption_key
    ):
        raise HTTPException(status_code=503, detail="Spotify todavía no está configurado")


def cipher(settings: Settings) -> Fernet:
    require_spotify_config(settings)
    try:
        return Fernet(settings.integration_encryption_key.encode())
    except (ValueError, TypeError) as error:
        raise HTTPException(
            status_code=503, detail="La protección de Spotify no está configurada"
        ) from error


def encrypt_token(value: str, settings: Settings) -> str:
    return cipher(settings).encrypt(value.encode()).decode()


def decrypt_token(value: str, settings: Settings) -> str:
    try:
        return cipher(settings).decrypt(value.encode()).decode()
    except InvalidToken as error:
        raise HTTPException(
            status_code=503, detail="No pudimos leer la conexión de Spotify"
        ) from error


def basic_auth(settings: Settings) -> str:
    require_spotify_config(settings)
    raw = f"{settings.spotify_client_id}:{settings.spotify_client_secret}".encode()
    return f"Basic {base64.b64encode(raw).decode()}"


def exchange_code(code: str, settings: Settings) -> dict:
    response = httpx.post(
        SPOTIFY_TOKEN_URL,
        data={
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": settings.spotify_redirect_uri,
        },
        headers={
            "Authorization": basic_auth(settings),
            "Content-Type": "application/x-www-form-urlencoded",
        },
        timeout=12,
    )
    if response.status_code != 200:
        raise HTTPException(status_code=502, detail="Spotify rechazó la autorización")
    return response.json()


def refresh_access_token(
    db: Session, connection: SpotifyConnection, settings: Settings
) -> str:
    response = httpx.post(
        SPOTIFY_TOKEN_URL,
        data={
            "grant_type": "refresh_token",
            "refresh_token": decrypt_token(connection.refresh_token_encrypted, settings),
        },
        headers={
            "Authorization": basic_auth(settings),
            "Content-Type": "application/x-www-form-urlencoded",
        },
        timeout=12,
    )
    if response.status_code != 200:
        connection.active = False
        db.commit()
        raise HTTPException(
            status_code=401, detail="Vuelve a conectar tu cuenta de Spotify"
        )
    payload = response.json()
    connection.access_token_encrypted = encrypt_token(payload["access_token"], settings)
    if payload.get("refresh_token"):
        connection.refresh_token_encrypted = encrypt_token(
            payload["refresh_token"], settings
        )
    connection.token_expires_at = datetime.now(timezone.utc) + timedelta(
        seconds=max(int(payload.get("expires_in", 3600)) - 30, 30)
    )
    connection.scope = payload.get("scope", connection.scope)
    db.commit()
    return payload["access_token"]


def access_token(
    db: Session, connection: SpotifyConnection, settings: Settings
) -> str:
    if utc(connection.token_expires_at) <= datetime.now(timezone.utc) + timedelta(
        seconds=30
    ):
        return refresh_access_token(db, connection, settings)
    return decrypt_token(connection.access_token_encrypted, settings)


def spotify_request(
    db: Session,
    connection: SpotifyConnection,
    settings: Settings,
    method: str,
    path: str,
) -> httpx.Response:
    token = access_token(db, connection, settings)
    response = httpx.request(
        method,
        f"{SPOTIFY_API}{path}",
        headers={"Authorization": f"Bearer {token}"},
        timeout=12,
    )
    if response.status_code == 401:
        connection.token_expires_at = datetime.now(timezone.utc)
        db.commit()
        token = refresh_access_token(db, connection, settings)
        response = httpx.request(
            method,
            f"{SPOTIFY_API}{path}",
            headers={"Authorization": f"Bearer {token}"},
            timeout=12,
        )
    return response


def player_state(
    db: Session, connection: SpotifyConnection, settings: Settings
) -> dict:
    response = spotify_request(db, connection, settings, "GET", "/me/player")
    if response.status_code == 204:
        return {"available": False, "is_playing": False}
    if response.status_code != 200:
        return {"available": False, "is_playing": False}
    payload = response.json()
    item = payload.get("item") or {}
    artists = item.get("artists") or []
    duration = max(int(item.get("duration_ms") or 1), 1)
    progress_ms = int(payload.get("progress_ms") or 0)
    progress = min(int((progress_ms * 100) / duration), 100)
    album = item.get("album") or {}
    return {
        "available": bool(item),
        "is_playing": bool(payload.get("is_playing")),
        "track": item.get("name") or "Sin reproducción",
        "artist": ", ".join(artist.get("name", "") for artist in artists if artist.get("name"))
        or "Abre Spotify en tu teléfono",
        "album": album.get("name") or "",
        "progress": progress,
        "progress_ms": progress_ms,
        "duration_ms": duration,
    }


def format_millis(value: int) -> str:
    total_seconds = max(value, 0) // 1000
    return f"{total_seconds // 60}:{total_seconds % 60:02d}"


def spotify_document(revision: int, state: dict) -> dict:
    title = str(state.get("track") or "Sin reproducción")[:26]
    artist = str(state.get("artist") or "Abre Spotify en tu teléfono")[:38]
    album = str(state.get("album") or "Spotify Connect")[:38]
    playing = bool(state.get("is_playing"))
    owner_name = str(state.get("owner_name") or "Leopold")[:18].upper()
    bluetooth_name = str(state.get("bluetooth_name") or "Leopold")[:24]
    return {
        "schema_version": 1,
        "revision": revision,
        "poll_after_ms": 5000,
        "screens": [
        {
            "id": "home",
            "background": "#000000",
            "components": [
                {
                    "type": "text",
                    "id": "home-owner",
                    "text": f"HOLA, {owner_name}",
                    "x": 233,
                    "y": 105,
                    "align": "center",
                    "size": 3,
                    "color": "#FFFFFF",
                },
                {
                    "type": "text",
                    "id": "home-title",
                    "text": "SOY TU ASISTENTE",
                    "x": 233,
                    "y": 175,
                    "align": "center",
                    "size": 2,
                    "color": "#FD9D20",
                },
                {
                    "type": "text",
                    "id": "home-hint",
                    "text": "DESLIZA PARA EXPLORAR",
                    "x": 233,
                    "y": 330,
                    "align": "center",
                    "size": 1,
                    "color": "#686868",
                },
            ],
        },
        {
            "id": "spotify-player",
            "background": "#000000",
            "components": [
                {
                    "type": "text",
                    "id": "track",
                    "text": title,
                    "x": 233,
                    "y": 72,
                    "align": "center",
                    "size": 2,
                    "color": "#FFFFFF",
                },
                {
                    "type": "text",
                    "id": "artist",
                    "text": artist,
                    "x": 233,
                    "y": 113,
                    "align": "center",
                    "size": 1,
                    "color": "#A7A7A7",
                },
                {
                    "type": "text",
                    "id": "album",
                    "text": album,
                    "x": 233,
                    "y": 141,
                    "align": "center",
                    "size": 1,
                    "color": "#686868",
                },
                {
                    "type": "progress",
                    "id": "progress",
                    "value": int(state.get("progress") or 0),
                    "x": 73,
                    "y": 202,
                    "width": 320,
                    "color": "#1DB954",
                },
                {
                    "type": "text",
                    "id": "elapsed",
                    "text": format_millis(int(state.get("progress_ms") or 0)),
                    "x": 73,
                    "y": 231,
                    "align": "left",
                    "size": 1,
                    "color": "#A7A7A7",
                },
                {
                    "type": "text",
                    "id": "duration",
                    "text": format_millis(int(state.get("duration_ms") or 0)),
                    "x": 393,
                    "y": 231,
                    "align": "right",
                    "size": 1,
                    "color": "#A7A7A7",
                },
                {
                    "type": "button",
                    "id": "spotify-prev",
                    "label": "Anterior",
                    "icon": "previous",
                    "x": 70,
                    "y": 294,
                    "width": 72,
                    "height": 72,
                    "color": "#FFFFFF",
                },
                {
                    "type": "button",
                    "id": "spotify-toggle",
                    "label": "Pausa" if playing else "Reproducir",
                    "icon": "pause" if playing else "play",
                    "x": 191,
                    "y": 288,
                    "width": 84,
                    "height": 84,
                    "color": "#1DB954",
                },
                {
                    "type": "button",
                    "id": "spotify-next",
                    "label": "Siguiente",
                    "icon": "next",
                    "x": 324,
                    "y": 294,
                    "width": 72,
                    "height": 72,
                    "color": "#FFFFFF",
                },
            ],
        },
        {
            "id": "bluetooth",
            "background": "#000000",
            "components": [
                {
                    "type": "icon",
                    "id": "bluetooth-icon",
                    "name": "bluetooth",
                    "x": 233,
                    "y": 130,
                    "size": 96,
                    "color": "#3D7FFF",
                },
                {
                    "type": "text",
                    "id": "bluetooth-title",
                    "text": "BLUETOOTH",
                    "x": 233,
                    "y": 220,
                    "align": "center",
                    "size": 3,
                    "color": "#FFFFFF",
                },
                {
                    "type": "text",
                    "id": "bluetooth-name",
                    "text": bluetooth_name,
                    "x": 233,
                    "y": 274,
                    "align": "center",
                    "size": 2,
                    "color": "#A7A7A7",
                },
                {
                    "type": "text",
                    "id": "bluetooth-state",
                    "text": "VISIBLE",
                    "x": 233,
                    "y": 326,
                    "align": "center",
                    "size": 1,
                    "color": "#3D7FFF",
                },
            ],
        },
        ],
    }


def welcome_document(revision: int) -> dict:
    return {
        "schema_version": 1,
        "revision": revision,
        "poll_after_ms": 5000,
        "screens": [{
            "id": "leopold-welcome",
            "background": "#000000",
            "components": [],
        }],
    }


def publish_document(db: Session, device: Device, document_factory) -> None:
    previous = db.scalar(
        select(DeviceUIDocument).where(
            DeviceUIDocument.device_id == device.id,
            DeviceUIDocument.is_active.is_(True),
        )
    )
    revision = (previous.revision if previous else 0) + 1
    if previous:
        previous.is_active = False
        db.flush()
    db.add(
        DeviceUIDocument(
            device_id=device.id,
            revision=revision,
            document=document_factory(revision),
            is_active=True,
        )
    )


def publish_spotify_ui(
    db: Session, user: User, connection: SpotifyConnection, settings: Settings
) -> None:
    state = player_state(db, connection, settings)
    state["owner_name"] = user.display_name
    for device in user.devices:
        device_state = {
            **state,
            "bluetooth_name": f"Leopold-{device.device_id[-4:]}",
        }
        publish_document(
            db,
            device,
            lambda revision, current=device_state: spotify_document(
                revision,
                current,
            ),
        )
    db.commit()


def publish_welcome_ui(db: Session, user: User) -> None:
    for device in user.devices:
        publish_document(db, device, welcome_document)
    db.commit()


def handle_device_spotify_event(
    db: Session,
    device: Device,
    component_id: str,
    settings: Settings,
) -> bool:
    connection = device.owner.spotify_connection if device.owner else None
    if connection is None or not connection.active:
        return False
    state = player_state(db, connection, settings)
    if component_id == "spotify-prev":
        path, method = "/me/player/previous", "POST"
    elif component_id == "spotify-next":
        path, method = "/me/player/next", "POST"
    elif component_id == "spotify-toggle":
        path, method = (
            ("/me/player/pause", "PUT")
            if state.get("is_playing")
            else ("/me/player/play", "PUT")
        )
    else:
        return False
    response = spotify_request(db, connection, settings, method, path)
    if response.status_code not in (200, 202, 204):
        if response.status_code == 403:
            raise HTTPException(
                status_code=403,
                detail="Los controles de reproducción requieren Spotify Premium",
            )
        if response.status_code == 404:
            raise HTTPException(
                status_code=409,
                detail="Abre Spotify en un dispositivo para iniciar la reproducción",
            )
        raise HTTPException(status_code=502, detail="Spotify no pudo ejecutar el control")
    publish_spotify_ui(db, device.owner, connection, settings)
    return True

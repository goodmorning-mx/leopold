from datetime import datetime, timedelta, timezone

import httpx
from fastapi import HTTPException
from sqlalchemy.orm import Session

from app.config import Settings
from app.models import Device, User, YouTubeConnection
from app.spotify import decrypt_token, encrypt_token, publish_document

YOUTUBE_API = "https://www.googleapis.com/youtube/v3"
GOOGLE_TOKEN_URL = "https://oauth2.googleapis.com/token"
YOUTUBE_SCOPE = "https://www.googleapis.com/auth/youtube.readonly"


def utc(value: datetime) -> datetime:
    return value if value.tzinfo else value.replace(tzinfo=timezone.utc)


def require_youtube_config(settings: Settings) -> None:
    if not (
        settings.youtube_client_id
        and settings.youtube_client_secret
        and settings.integration_encryption_key
    ):
        raise HTTPException(status_code=503, detail="YouTube todavía no está configurado")


def exchange_code(code: str, settings: Settings) -> dict:
    require_youtube_config(settings)
    response = httpx.post(
        GOOGLE_TOKEN_URL,
        data={
            "code": code,
            "client_id": settings.youtube_client_id,
            "client_secret": settings.youtube_client_secret,
            "redirect_uri": settings.youtube_redirect_uri,
            "grant_type": "authorization_code",
        },
        timeout=12,
    )
    if response.status_code != 200:
        raise HTTPException(status_code=502, detail="Google rechazó la autorización de YouTube")
    return response.json()


def refresh_access_token(
    db: Session, connection: YouTubeConnection, settings: Settings
) -> str:
    require_youtube_config(settings)
    response = httpx.post(
        GOOGLE_TOKEN_URL,
        data={
            "client_id": settings.youtube_client_id,
            "client_secret": settings.youtube_client_secret,
            "refresh_token": decrypt_token(connection.refresh_token_encrypted, settings),
            "grant_type": "refresh_token",
        },
        timeout=12,
    )
    if response.status_code != 200:
        connection.active = False
        db.commit()
        raise HTTPException(status_code=401, detail="Vuelve a conectar tu cuenta de YouTube")
    payload = response.json()
    connection.access_token_encrypted = encrypt_token(payload["access_token"], settings)
    connection.token_expires_at = datetime.now(timezone.utc) + timedelta(
        seconds=max(int(payload.get("expires_in", 3600)) - 30, 30)
    )
    connection.scope = payload.get("scope", connection.scope)
    db.commit()
    return payload["access_token"]


def access_token(
    db: Session, connection: YouTubeConnection, settings: Settings
) -> str:
    if utc(connection.token_expires_at) <= datetime.now(timezone.utc) + timedelta(seconds=30):
        return refresh_access_token(db, connection, settings)
    return decrypt_token(connection.access_token_encrypted, settings)


def youtube_request(
    db: Session,
    connection: YouTubeConnection,
    settings: Settings,
    path: str,
    params: dict[str, str | int | bool],
) -> httpx.Response:
    token = access_token(db, connection, settings)
    response = httpx.get(
        f"{YOUTUBE_API}{path}",
        params=params,
        headers={"Authorization": f"Bearer {token}"},
        timeout=12,
    )
    if response.status_code == 401:
        connection.token_expires_at = datetime.now(timezone.utc)
        db.commit()
        token = refresh_access_token(db, connection, settings)
        response = httpx.get(
            f"{YOUTUBE_API}{path}",
            params=params,
            headers={"Authorization": f"Bearer {token}"},
            timeout=12,
        )
    return response


def channel_profile(token: str) -> dict:
    response = httpx.get(
        f"{YOUTUBE_API}/channels",
        params={"part": "snippet", "mine": "true", "maxResults": 1},
        headers={"Authorization": f"Bearer {token}"},
        timeout=12,
    )
    if response.status_code != 200:
        raise HTTPException(status_code=502, detail="No pudimos leer tu canal de YouTube")
    items = response.json().get("items") or []
    if not items:
        raise HTTPException(status_code=409, detail="Esta cuenta no tiene un canal de YouTube")
    return items[0]


def youtube_feed(
    db: Session, connection: YouTubeConnection, settings: Settings
) -> list[dict[str, str]]:
    response = youtube_request(
        db,
        connection,
        settings,
        "/activities",
        {"part": "snippet,contentDetails", "home": "true", "maxResults": 5},
    )
    if response.status_code != 200:
        return []
    feed: list[dict[str, str]] = []
    for item in response.json().get("items") or []:
        snippet = item.get("snippet") or {}
        title = str(snippet.get("title") or "Nuevo video")[:42]
        channel = str(snippet.get("channelTitle") or "YouTube")[:28]
        feed.append({"title": title, "channel": channel})
        if len(feed) == 3:
            break
    return feed


def youtube_screen(feed: list[dict[str, str]], display_name: str | None) -> dict:
    components: list[dict] = [
        {
            "type": "text", "id": "youtube-brand", "text": "YOUTUBE",
            "x": 233, "y": 58, "align": "center", "size": 2, "color": "#FF453A",
        },
        {
            "type": "text", "id": "youtube-account",
            "text": (display_name or "Tu cuenta")[:32],
            "x": 233, "y": 92, "align": "center", "size": 1, "color": "#A7A7A7",
        },
    ]
    entries = feed or [{"title": "Abre YouTube para descubrir videos", "channel": "Sin actividad reciente"}]
    for index, item in enumerate(entries[:3]):
        y = 140 + index * 76
        components.extend(
            [
                {
                    "type": "text", "id": f"youtube-title-{index}",
                    "text": item["title"], "x": 78, "y": y,
                    "align": "left", "size": 1, "color": "#FFFFFF",
                },
                {
                    "type": "text", "id": f"youtube-channel-{index}",
                    "text": item["channel"], "x": 78, "y": y + 27,
                    "align": "left", "size": 1, "color": "#8E8E93",
                },
            ]
        )
    components.append(
        {
            "type": "button", "id": "youtube-refresh", "label": "Actualizar",
            "x": 133, "y": 378, "width": 200, "height": 58, "color": "#FF453A",
        }
    )
    return {"id": "youtube-feed", "background": "#000000", "components": components}


def youtube_document(
    revision: int,
    feed: list[dict[str, str]],
    display_name: str | None,
    owner_name: str,
) -> dict:
    return {
        "schema_version": 1,
        "revision": revision,
        "poll_after_ms": 15000,
        "screens": [
            {
                "id": "home", "background": "#000000",
                "components": [
                    {
                        "type": "text", "id": "home-owner",
                        "text": f"HOLA, {owner_name[:18].upper()}",
                        "x": 233, "y": 105, "align": "center", "size": 3,
                        "color": "#FFFFFF",
                    },
                    {
                        "type": "text", "id": "home-hint",
                        "text": "DESLIZA PARA YOUTUBE", "x": 233, "y": 330,
                        "align": "center", "size": 1, "color": "#686868",
                    },
                    {
                        "type": "button", "id": "open-computer", "label": "COMPUTER",
                        "x": 113, "y": 265, "width": 240, "height": 58,
                        "color": "#3D7FFF",
                    },
                ],
            },
            youtube_screen(feed, display_name),
        ],
    }


def publish_youtube_ui(
    db: Session, user: User, connection: YouTubeConnection, settings: Settings
) -> None:
    feed = youtube_feed(db, connection, settings)
    connection.cached_feed = feed
    connection.feed_updated_at = datetime.now(timezone.utc)
    for device in user.devices:
        publish_document(
            db,
            device,
            lambda revision: youtube_document(
                revision, feed, connection.display_name, user.display_name
            ),
        )
    db.commit()


def handle_device_youtube_event(
    db: Session, device: Device, component_id: str, settings: Settings
) -> bool:
    if component_id != "youtube-refresh" or not device.owner:
        return False
    connection = device.owner.youtube_connection
    if connection is None or not connection.active:
        return False
    publish_youtube_ui(db, device.owner, connection, settings)
    return True

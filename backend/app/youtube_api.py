import secrets
from datetime import datetime, timedelta, timezone
from typing import Annotated
from urllib.parse import urlencode

import httpx
from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import RedirectResponse
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.auth_api import AuthenticatedUser, token_hash
from app.config import Settings, get_settings
from app.database import get_db
from app.models import User, YouTubeConnection, YouTubeOAuthState
from app.spotify import decrypt_token, encrypt_token, publish_welcome_ui
from app.youtube import (
    YOUTUBE_SCOPE,
    channel_profile,
    exchange_code,
    publish_youtube_ui,
    require_youtube_config,
)

router = APIRouter(prefix="/v1/integrations/youtube")
Db = Annotated[Session, Depends(get_db)]
Config = Annotated[Settings, Depends(get_settings)]


@router.get("/status")
def youtube_status(user: AuthenticatedUser, settings: Config) -> dict:
    configured = bool(
        settings.youtube_client_id
        and settings.youtube_client_secret
        and settings.integration_encryption_key
    )
    connection = user.youtube_connection
    return {
        "configured": configured,
        "connected": bool(connection and connection.active),
        "display_name": connection.display_name if connection else None,
    }


@router.get("/connect")
def youtube_connect(
    db: Db, user: AuthenticatedUser, settings: Config
) -> RedirectResponse:
    require_youtube_config(settings)
    raw_state = secrets.token_urlsafe(32)
    db.add(
        YouTubeOAuthState(
            user_id=user.id,
            token_hash=token_hash(raw_state),
            expires_at=datetime.now(timezone.utc) + timedelta(minutes=10),
        )
    )
    db.commit()
    query = urlencode(
        {
            "client_id": settings.youtube_client_id,
            "redirect_uri": settings.youtube_redirect_uri,
            "response_type": "code",
            "scope": YOUTUBE_SCOPE,
            "access_type": "offline",
            "include_granted_scopes": "true",
            "prompt": "consent",
            "state": raw_state,
        }
    )
    return RedirectResponse(f"https://accounts.google.com/o/oauth2/v2/auth?{query}")


@router.get("/callback", include_in_schema=False)
def youtube_callback(
    db: Db,
    settings: Config,
    state: str | None = None,
    code: str | None = None,
    error: str | None = None,
) -> RedirectResponse:
    frontend = settings.allowed_origins[0].rstrip("/")
    if error:
        return RedirectResponse(f"{frontend}/?youtube=denied")
    if not state or not code:
        raise HTTPException(status_code=400, detail="Respuesta de YouTube incompleta")
    oauth_state = db.scalar(
        select(YouTubeOAuthState).where(
            YouTubeOAuthState.token_hash == token_hash(state)
        )
    )
    now = datetime.now(timezone.utc)
    expires_at = (
        oauth_state.expires_at
        if oauth_state and oauth_state.expires_at.tzinfo
        else oauth_state.expires_at.replace(tzinfo=timezone.utc)
        if oauth_state
        else now
    )
    if oauth_state is None or expires_at <= now:
        raise HTTPException(status_code=400, detail="La sesión de YouTube expiró")
    user = db.get(User, oauth_state.user_id)
    db.delete(oauth_state)
    token = exchange_code(code, settings)
    access = token["access_token"]
    channel = channel_profile(access)
    snippet = channel.get("snippet") or {}
    connection = db.scalar(
        select(YouTubeConnection).where(YouTubeConnection.user_id == user.id)
    )
    if connection is None:
        connection = YouTubeConnection(
            user_id=user.id,
            channel_id=channel["id"],
            display_name=snippet.get("title"),
            access_token_encrypted="",
            refresh_token_encrypted="",
            token_expires_at=now,
            scope="",
        )
        db.add(connection)
    connection.channel_id = channel["id"]
    connection.display_name = snippet.get("title")
    connection.access_token_encrypted = encrypt_token(access, settings)
    refresh = token.get("refresh_token")
    if refresh:
        connection.refresh_token_encrypted = encrypt_token(refresh, settings)
    if not connection.refresh_token_encrypted:
        raise HTTPException(status_code=502, detail="Google no entregó acceso renovable")
    connection.token_expires_at = now + timedelta(
        seconds=max(int(token.get("expires_in", 3600)) - 30, 30)
    )
    connection.scope = token.get("scope", YOUTUBE_SCOPE)
    connection.active = True
    db.flush()
    publish_youtube_ui(db, user, connection, settings)
    return RedirectResponse(f"{frontend}/?youtube=connected")


@router.delete("", status_code=204)
def youtube_disconnect(db: Db, user: AuthenticatedUser, settings: Config) -> None:
    connection = user.youtube_connection
    if connection is not None:
        try:
            token = decrypt_token(connection.refresh_token_encrypted, settings)
            httpx.post(
                "https://oauth2.googleapis.com/revoke",
                data={"token": token},
                headers={"Content-Type": "application/x-www-form-urlencoded"},
                timeout=8,
            )
        except (HTTPException, httpx.HTTPError):
            pass
        db.delete(connection)
        db.flush()
    publish_welcome_ui(db, user)

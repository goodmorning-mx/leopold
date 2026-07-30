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
from app.models import SpotifyConnection, SpotifyOAuthState, User
from app.spotify import (
    SPOTIFY_API,
    SPOTIFY_SCOPES,
    encrypt_token,
    exchange_code,
    publish_spotify_ui,
    publish_welcome_ui,
    require_spotify_config,
)

router = APIRouter(prefix="/v1/integrations/spotify")
Db = Annotated[Session, Depends(get_db)]
Config = Annotated[Settings, Depends(get_settings)]


@router.get("/status")
def spotify_status(user: AuthenticatedUser, settings: Config) -> dict:
    configured = bool(
        settings.spotify_client_id
        and settings.spotify_client_secret
        and settings.integration_encryption_key
    )
    connection = user.spotify_connection
    return {
        "configured": configured,
        "connected": bool(connection and connection.active),
        "display_name": connection.display_name if connection else None,
    }


@router.get("/connect")
def spotify_connect(
    db: Db, user: AuthenticatedUser, settings: Config
) -> RedirectResponse:
    require_spotify_config(settings)
    raw_state = secrets.token_urlsafe(32)
    db.add(
        SpotifyOAuthState(
            user_id=user.id,
            token_hash=token_hash(raw_state),
            expires_at=datetime.now(timezone.utc) + timedelta(minutes=10),
        )
    )
    db.commit()
    query = urlencode(
        {
            "response_type": "code",
            "client_id": settings.spotify_client_id,
            "scope": SPOTIFY_SCOPES,
            "redirect_uri": settings.spotify_redirect_uri,
            "state": raw_state,
        }
    )
    return RedirectResponse(f"https://accounts.spotify.com/authorize?{query}")


@router.get("/callback", include_in_schema=False)
def spotify_callback(
    db: Db,
    settings: Config,
    state: str | None = None,
    code: str | None = None,
    error: str | None = None,
) -> RedirectResponse:
    frontend = settings.allowed_origins[0].rstrip("/")
    if error:
        return RedirectResponse(f"{frontend}/?spotify=denied")
    if not state or not code:
        raise HTTPException(status_code=400, detail="Respuesta de Spotify incompleta")
    oauth_state = db.scalar(
        select(SpotifyOAuthState).where(
            SpotifyOAuthState.token_hash == token_hash(state)
        )
    )
    now = datetime.now(timezone.utc)
    if oauth_state is None or (
        oauth_state.expires_at if oauth_state.expires_at.tzinfo else oauth_state.expires_at.replace(tzinfo=timezone.utc)
    ) <= now:
        raise HTTPException(status_code=400, detail="La sesión de Spotify expiró")
    user = db.get(User, oauth_state.user_id)
    db.delete(oauth_state)
    token = exchange_code(code, settings)
    access = token["access_token"]
    profile = httpx.get(
        f"{SPOTIFY_API}/me",
        headers={"Authorization": f"Bearer {access}"},
        timeout=12,
    )
    if profile.status_code != 200:
        raise HTTPException(status_code=502, detail="No pudimos leer tu perfil de Spotify")
    spotify_user = profile.json()
    connection = db.scalar(
        select(SpotifyConnection).where(SpotifyConnection.user_id == user.id)
    )
    if connection is None:
        connection = SpotifyConnection(
            user_id=user.id,
            spotify_user_id=spotify_user["id"],
            display_name=spotify_user.get("display_name"),
            access_token_encrypted="",
            refresh_token_encrypted="",
            token_expires_at=now,
            scope="",
        )
        db.add(connection)
    connection.spotify_user_id = spotify_user["id"]
    connection.display_name = spotify_user.get("display_name")
    connection.access_token_encrypted = encrypt_token(access, settings)
    refresh = token.get("refresh_token")
    if refresh:
        connection.refresh_token_encrypted = encrypt_token(refresh, settings)
    if not connection.refresh_token_encrypted:
        raise HTTPException(status_code=502, detail="Spotify no entregó acceso renovable")
    connection.token_expires_at = now + timedelta(
        seconds=max(int(token.get("expires_in", 3600)) - 30, 30)
    )
    connection.scope = token.get("scope", "")
    connection.active = True
    db.flush()
    publish_spotify_ui(db, user, connection, settings)
    return RedirectResponse(f"{frontend}/?spotify=connected")


@router.delete("", status_code=204)
def spotify_disconnect(db: Db, user: AuthenticatedUser) -> None:
    connection = user.spotify_connection
    if connection is not None:
        db.delete(connection)
        db.flush()
    publish_welcome_ui(db, user)

from cryptography.fernet import Fernet
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.config import Settings, get_settings
from app.database import get_db
from app.main import app
from app.models import Base
from app.spotify import spotify_document, welcome_document
from app.schemas import UIDocument


def test_spotify_documents_follow_sdui_contract() -> None:
    playing = spotify_document(
        4,
        {
            "track": "A Song",
            "artist": "An Artist",
            "album": "An Album",
            "progress": 42,
            "progress_ms": 61000,
            "duration_ms": 180000,
            "is_playing": True,
            "owner_name": "Abel",
        },
    )
    assert playing["screen"]["id"] == "spotify-player"
    UIDocument.model_validate(playing)
    assert len(playing["screen"]["components"]) == 10
    assert playing["screen"]["components"][0]["text"] == "HOLA, ABEL"
    assert next(
        item
        for item in playing["screen"]["components"]
        if item["id"] == "spotify-toggle"
    )["icon"] == "pause"
    assert welcome_document(5)["screen"]["id"] == "leopold-welcome"


def test_spotify_status_and_connect_require_server_configuration() -> None:
    engine = create_engine(
        "sqlite+pysqlite:///:memory:",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )
    Base.metadata.create_all(engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)

    def override_db():
        with factory() as session:
            yield session

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_settings] = lambda: Settings(
        DATABASE_URL="sqlite+pysqlite:///:memory:",
        CORS_ORIGINS="http://localhost:5173",
        LEOPOLD_ADMIN_PROVISIONING_TOKEN="test-admin",
    )
    with TestClient(app) as client:
        registered = client.post(
            "/v1/auth/register",
            json={
                "email": "spotify@example.com",
                "display_name": "Spotify User",
                "password": "secure-password",
            },
        )
        assert registered.status_code == 201
        assert client.get("/v1/integrations/spotify/status").json() == {
            "configured": False,
            "connected": False,
            "display_name": None,
        }
        assert client.get("/v1/integrations/spotify/connect").status_code == 503

    app.dependency_overrides.clear()
    Base.metadata.drop_all(engine)
    engine.dispose()


def test_valid_encryption_key_is_accepted() -> None:
    settings = Settings(
        SPOTIFY_CLIENT_ID="client",
        SPOTIFY_CLIENT_SECRET="secret",
        LEOPOLD_INTEGRATION_ENCRYPTION_KEY=Fernet.generate_key().decode(),
    )
    assert settings.spotify_client_id == "client"

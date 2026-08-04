from cryptography.fernet import Fernet
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.config import Settings, get_settings
from app.database import get_db
from app.main import app
from app.models import Base
from app.schemas import UIDocument
from app.youtube import youtube_document


def test_youtube_document_follows_sdui_contract() -> None:
    document = youtube_document(
        8,
        [
            {"title": "A new video", "channel": "Creator One"},
            {"title": "Another video", "channel": "Creator Two"},
        ],
        "My channel",
        "Abel",
    )
    validated = UIDocument.model_validate(document)
    assert [screen.id for screen in validated.screens] == ["home", "youtube-feed"]
    assert validated.screens[1].components[-1].id == "youtube-refresh"
    assert validated.screens[0].components[0].text == "HOLA, ABEL"


def test_youtube_status_and_connect_require_server_configuration() -> None:
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
                "email": "youtube@example.com",
                "display_name": "YouTube User",
                "password": "secure-password",
            },
        )
        assert registered.status_code == 201
        assert client.get("/v1/integrations/youtube/status").json() == {
            "configured": False,
            "connected": False,
            "display_name": None,
        }
        assert client.get("/v1/integrations/youtube/connect").status_code == 503

    app.dependency_overrides.clear()
    Base.metadata.drop_all(engine)
    engine.dispose()


def test_youtube_settings_accept_valid_encryption_key() -> None:
    settings = Settings(
        YOUTUBE_CLIENT_ID="client",
        YOUTUBE_CLIENT_SECRET="secret",
        YOUTUBE_REDIRECT_URI="https://api.example.test/v1/integrations/youtube/callback",
        LEOPOLD_INTEGRATION_ENCRYPTION_KEY=Fernet.generate_key().decode(),
    )
    assert settings.youtube_client_id == "client"
    assert settings.youtube_redirect_uri.startswith("https://")

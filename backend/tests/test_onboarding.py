import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.config import Settings, get_settings
from app.database import get_db
from app.main import app
from app.models import Base


@pytest.fixture
def client():
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
        LEOPOLD_ADMIN_PROVISIONING_TOKEN="local-test-admin-token",
    )
    with TestClient(app) as test_client:
        yield test_client
    app.dependency_overrides.clear()
    Base.metadata.drop_all(engine)
    engine.dispose()


def provision(client: TestClient) -> dict:
    response = client.post(
        "/v1/admin/devices",
        headers={"Authorization": "Bearer local-test-admin-token"},
        json={"label": "Leopold onboarding"},
    )
    assert response.status_code == 201
    return response.json()


def device_headers(credentials: dict) -> dict[str, str]:
    return {
        "X-Leopold-Device-ID": credentials["device_id"],
        "Authorization": f"Bearer {credentials['device_secret']}",
    }


def test_qr_onboarding_registers_user_and_claims_device(client) -> None:
    credentials = provision(client)
    setup_token = "0123456789abcdef0123456789abcdef"

    pending = client.get(f"/v1/onboarding/{setup_token}")
    assert pending.status_code == 200
    assert pending.json()["pending"] is True
    assert pending.headers["cache-control"] == "no-store, max-age=0"
    continuation = client.get(
        f"/v1/onboarding/{setup_token}/continue?ssid=Leopold-7817",
        follow_redirects=False,
    )
    assert continuation.status_code == 307
    assert continuation.headers["cache-control"] == "no-store, max-age=0"
    assert continuation.headers["location"] == (
        f"http://localhost:5173/setup/{setup_token}?ssid=Leopold-7817"
    )

    connected = client.post(
        "/v1/device/connect",
        headers=device_headers(credentials),
        json={
            "firmware_version": "0.3.0",
            "setup_token": setup_token,
            "capabilities": {"display": "466x466", "touch": True},
        },
    )
    assert connected.status_code == 200
    payload = connected.json()
    assert payload["claimed"] is False
    assert payload["owner_display_name"] is None
    assert payload["onboarding_url"] == (
        f"http://localhost:5173/setup/{setup_token}"
    )
    token = payload["onboarding_url"].rsplit("/", 1)[1]

    status = client.get(f"/v1/onboarding/{token}")
    assert status.status_code == 200
    assert status.json()["valid"] is True
    assert status.json()["claimed"] is False
    assert status.json()["pending"] is False

    registered = client.post(
        "/v1/auth/register",
        json={
            "email": "owner@example.com",
            "display_name": "Owner",
            "password": "correct horse battery staple",
        },
    )
    assert registered.status_code == 201
    assert registered.json() == {
        "email": "owner@example.com",
        "display_name": "Owner",
    }
    assert registered.cookies.get("leopold_session")

    claimed = client.post(f"/v1/onboarding/{token}/claim")
    assert claimed.status_code == 200
    assert claimed.json()["claimed"] is True

    device_status = client.post(
        "/v1/device/onboarding/status",
        headers=device_headers(credentials),
        json={},
    )
    assert device_status.status_code == 200
    assert device_status.json() == {
        "claimed": True,
        "owner_display_name": "Owner",
    }

    reconnected = client.post(
        "/v1/device/connect",
        headers=device_headers(credentials),
        json={"firmware_version": "0.3.1", "capabilities": {}},
    )
    assert reconnected.status_code == 200
    assert reconnected.json()["claimed"] is True
    assert reconnected.json()["owner_display_name"] == "Owner"
    assert reconnected.json()["onboarding_url"] is None

    reconnect_token = "fedcba9876543210fedcba9876543210"
    restored = client.post(
        "/v1/device/connect",
        headers=device_headers(credentials),
        json={
            "firmware_version": "0.5.0",
            "setup_token": reconnect_token,
            "capabilities": {},
        },
    )
    assert restored.status_code == 200
    restored_status = client.get(f"/v1/onboarding/{reconnect_token}")
    assert restored_status.status_code == 200
    assert restored_status.json()["claimed"] is True

    devices = client.get("/v1/me/devices")
    assert devices.status_code == 200
    assert len(devices.json()) == 1
    assert devices.json()[0]["device_id"] == credentials["device_id"]
    assert devices.json()[0]["label"] == "Leopold onboarding"

    removed = client.delete(f"/v1/me/devices/{credentials['device_id']}")
    assert removed.status_code == 204
    assert client.get("/v1/me/devices").json() == []
    assert (
        client.delete(f"/v1/me/devices/{credentials['device_id']}").status_code
        == 404
    )


def test_login_logout_and_claim_security(client) -> None:
    credentials = provision(client)
    connected = client.post(
        "/v1/device/connect",
        headers=device_headers(credentials),
        json={"firmware_version": "0.3.0", "capabilities": {}},
    )
    token = connected.json()["onboarding_url"].rsplit("/", 1)[1]

    assert client.post(f"/v1/onboarding/{token}/claim").status_code == 401
    assert (
        client.post(
            "/v1/auth/register",
            json={
                "email": "person@example.com",
                "display_name": "Person",
                "password": "secure-password",
            },
        ).status_code
        == 201
    )
    assert client.post("/v1/auth/logout").status_code == 204
    assert client.get("/v1/auth/me").status_code == 401
    assert (
        client.post(
            "/v1/auth/login",
            json={"email": "person@example.com", "password": "wrong-password"},
        ).status_code
        == 401
    )
    assert (
        client.post(
            "/v1/auth/login",
            json={"email": "person@example.com", "password": "secure-password"},
        ).status_code
        == 200
    )
    assert client.post(f"/v1/onboarding/{token}/claim").status_code == 200
    assert client.post(f"/v1/onboarding/{token}/claim").status_code == 410

import hashlib
import logging

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine, func, select
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.config import Settings, get_settings
from app.database import get_db
from app.main import app
from app.models import Base, Device, DeviceEvent, DeviceStatus

ADMIN_TOKEN = "local-test-admin-token"


@pytest.fixture
def session_factory():
    engine = create_engine(
        "sqlite+pysqlite:///:memory:",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )
    Base.metadata.create_all(engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)
    yield factory
    Base.metadata.drop_all(engine)
    engine.dispose()


@pytest.fixture
def client(session_factory):
    def override_db():
        with session_factory() as session:
            yield session

    def override_settings():
        return Settings(
            DATABASE_URL="sqlite+pysqlite:///:memory:",
            LEOPOLD_ADMIN_PROVISIONING_TOKEN=ADMIN_TOKEN,
        )

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_settings] = override_settings
    with TestClient(app) as test_client:
        yield test_client
    app.dependency_overrides.clear()


def provision(client: TestClient, label: str = "Leopold prototipo 01") -> dict:
    response = client.post(
        "/v1/admin/devices",
        headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
        json={"label": label},
    )
    assert response.status_code == 201
    return response.json()


def device_headers(credentials: dict, secret: str | None = None) -> dict[str, str]:
    return {
        "X-Leopold-Device-ID": credentials["device_id"],
        "Authorization": f"Bearer {secret or credentials['device_secret']}",
    }


def test_provisioning_creates_hashed_secret_and_initial_ui(
    client, session_factory, caplog
) -> None:
    caplog.set_level(logging.DEBUG)
    credentials = provision(client)

    assert credentials["device_id"].startswith("LPD-")
    assert len(credentials["device_id"]) == 10
    assert credentials["device_secret"]
    with session_factory() as db:
        device = db.scalar(
            select(Device).where(Device.device_id == credentials["device_id"])
        )
        assert device is not None
        assert (
            device.secret_hash
            == hashlib.sha256(credentials["device_secret"].encode()).hexdigest()
        )
        assert credentials["device_secret"] not in device.secret_hash
        assert len(device.ui_documents) == 1
        assert device.ui_documents[0].revision == 1
    assert credentials["device_secret"] not in caplog.text


def test_admin_token_missing_incorrect_and_unconfigured(client) -> None:
    assert client.post("/v1/admin/devices", json={"label": "test"}).status_code == 401
    assert (
        client.post(
            "/v1/admin/devices",
            headers={"Authorization": "Bearer incorrect"},
            json={"label": "test"},
        ).status_code
        == 401
    )
    app.dependency_overrides[get_settings] = lambda: Settings(
        DATABASE_URL="sqlite+pysqlite:///:memory:",
        LEOPOLD_ADMIN_PROVISIONING_TOKEN=None,
    )
    assert (
        client.post(
            "/v1/admin/devices",
            headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
            json={"label": "test"},
        ).status_code
        == 503
    )


def test_device_authentication_success_wrong_secret_and_unknown_id(client) -> None:
    credentials = provision(client)
    assert (
        client.get("/v1/device/ui", headers=device_headers(credentials)).status_code
        == 200
    )
    bad = client.get(
        "/v1/device/ui", headers=device_headers(credentials, secret="incorrect")
    )
    unknown = client.get(
        "/v1/device/ui",
        headers={
            "X-Leopold-Device-ID": "LPD-000000",
            "Authorization": "Bearer incorrect",
        },
    )
    assert bad.status_code == unknown.status_code == 401
    assert bad.json() == unknown.json()
    assert credentials["device_secret"] not in bad.text + unknown.text


def test_disabled_device_receives_403(client, session_factory) -> None:
    credentials = provision(client)
    with session_factory() as db:
        device = db.scalar(
            select(Device).where(Device.device_id == credentials["device_id"])
        )
        device.status = DeviceStatus.disabled
        db.commit()
    response = client.get("/v1/device/ui", headers=device_headers(credentials))
    assert response.status_code == 403


def test_connect_updates_presence_and_returns_intervals(
    client, session_factory
) -> None:
    credentials = provision(client)
    response = client.post(
        "/v1/device/connect",
        headers=device_headers(credentials),
        json={
            "firmware_version": "0.2.0",
            "capabilities": {
                "display": "466x466",
                "touch": True,
                "audio": False,
                "battery": True,
            },
        },
    )
    assert response.status_code == 200
    assert response.json()["ui_revision"] == 1
    assert response.json()["heartbeat_interval_seconds"] == 30
    with session_factory() as db:
        device = db.scalar(
            select(Device).where(Device.device_id == credentials["device_id"])
        )
        assert device.status == DeviceStatus.online
        assert device.firmware_version == "0.2.0"
        assert device.capabilities["audio"] is False
        assert device.last_seen_at is not None
        assert device.last_ip == "testclient"


def test_ui_is_valid_and_supports_etag(client) -> None:
    credentials = provision(client)
    response = client.get("/v1/device/ui", headers=device_headers(credentials))
    assert response.status_code == 200
    assert response.headers["etag"] == '"1"'
    document = response.json()
    assert document["schema_version"] == 1
    assert document["revision"] == 1
    assert {component["type"] for component in document["screen"]["components"]} == {
        "text",
        "indicator",
        "button",
    }
    unchanged = client.get(
        "/v1/device/ui",
        headers={**device_headers(credentials), "If-None-Match": '"1"'},
    )
    assert unchanged.status_code == 304
    assert unchanged.content == b""
    assert unchanged.headers["etag"] == '"1"'


def test_event_is_idempotent_and_payload_is_bounded(client, session_factory) -> None:
    credentials = provision(client)
    event = {
        "event_id": "evt-local-001",
        "revision": 1,
        "type": "tap",
        "component_id": "continue",
        "payload": {},
        "occurred_at": "2026-07-29T08:01:00Z",
    }
    first = client.post(
        "/v1/device/events", headers=device_headers(credentials), json=event
    )
    duplicate = client.post(
        "/v1/device/events", headers=device_headers(credentials), json=event
    )
    assert first.status_code == duplicate.status_code == 202
    with session_factory() as db:
        assert db.scalar(select(func.count()).select_from(DeviceEvent)) == 1
    oversized = {**event, "event_id": "evt-local-002", "payload": {"data": "x" * 5000}}
    assert (
        client.post(
            "/v1/device/events", headers=device_headers(credentials), json=oversized
        ).status_code
        == 422
    )


def test_heartbeat_updates_telemetry_without_returning_secret(
    client, session_factory
) -> None:
    credentials = provision(client)
    response = client.post(
        "/v1/device/heartbeat",
        headers=device_headers(credentials),
        json={
            "firmware_version": "0.2.0",
            "wifi_rssi": -54,
            "battery_percent": 82,
            "uptime_seconds": 150,
        },
    )
    assert response.status_code == 200
    assert response.json()["ok"] is True
    assert response.json()["ui_revision"] == 1
    assert credentials["device_secret"] not in response.text
    with session_factory() as db:
        device = db.scalar(
            select(Device).where(Device.device_id == credentials["device_id"])
        )
        assert (device.wifi_rssi, device.battery_percent, device.uptime_seconds) == (
            -54,
            82,
            150,
        )
        assert device.last_seen_at is not None

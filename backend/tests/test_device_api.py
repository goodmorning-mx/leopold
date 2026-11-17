import hashlib
import logging
from concurrent.futures import ThreadPoolExecutor
from threading import Barrier

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine, func, select
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.config import Settings, get_settings
from app.database import get_db
from app.main import app
from app.models import Base, Device, DeviceEvent, DeviceStatus, DeviceUIDocument

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


def replacement_screen(screen_id: str = "remote-home") -> dict:
    return {
        "id": screen_id,
        "background": "#001122",
        "components": [
            {
                "type": "text",
                "id": "remote-message",
                "text": "Pantalla actualizada",
                "x": 233,
                "y": 120,
                "align": "center",
                "size": 3,
                "color": "#FFFFFF",
            }
        ],
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
    assert response.headers["etag"] == '"controls-v1-1"'
    document = response.json()
    assert document["schema_version"] == 1
    assert document["revision"] == 1
    controls = next(
        screen for screen in document["screens"] if screen["id"] == "computer-controls"
    )
    assert {component["id"] for component in controls["components"] if component["type"] == "button"} == {
        "computer-volume-down",
        "computer-volume-mute",
        "computer-volume-up",
        "computer-screenshot",
    }
    assert {component["type"] for component in document["screens"][0]["components"]} == {
        "text",
        "indicator",
        "button",
    }
    usb_button = next(
        component for component in document["screens"][0]["components"]
        if component["id"] == "open-usb"
    )
    assert usb_button["icon"] == "usb"
    assert usb_button["width"] == usb_button["height"] == 72
    unchanged = client.get(
        "/v1/device/ui",
        headers={**device_headers(credentials), "If-None-Match": '"controls-v1-1"'},
    )
    assert unchanged.status_code == 304
    assert unchanged.content == b""
    assert unchanged.headers["etag"] == '"controls-v1-1"'


def test_device_asset_is_authenticated_and_matches_shared_manifest(client) -> None:
    credentials = provision(client)

    response = client.get(
        "/v1/device/assets/icon_clock.bin",
        headers=device_headers(credentials),
    )

    assert response.status_code == 200
    assert response.headers["content-type"].startswith("application/octet-stream")
    assert response.headers["x-leopold-asset-format"] == "rgb565a8"
    assert response.headers["x-leopold-asset-sha256"] == hashlib.sha256(response.content).hexdigest()
    assert response.headers["x-leopold-catalog-version"] == "1"
    assert response.headers["x-leopold-renderer-compatibility"] == "17"
    assert response.headers["etag"] == f'"sha256-{hashlib.sha256(response.content).hexdigest()}"'
    assert response.headers["cache-control"] == "private, no-cache"
    assert len(response.content) == 80 * 80 * 3

    unauthorized = client.get("/v1/device/assets/icon_clock.bin")
    assert unauthorized.status_code == 401

    unknown = client.get(
        "/v1/device/assets/not-from-the-manifest.bin",
        headers=device_headers(credentials),
    )
    assert unknown.status_code == 404


def test_device_asset_catalog_is_small_and_authenticated(client) -> None:
    credentials = provision(client)
    response = client.get("/v1/device/assets", headers=device_headers(credentials))
    assert response.status_code == 200
    body = response.json()
    assert body["catalog_version"] == 1
    assert body["renderer_compatibility"] == 17
    assert body["assets"]["open-clock"]["bytes"] == 80 * 80 * 3
    assert len(body["assets"]["open-clock"]["sha256"]) == 64
    assert client.get("/v1/device/assets").status_code == 401


def test_device_asset_host_rejects_corrupt_catalog_bytes(client, tmp_path) -> None:
    credentials = provision(client)
    (tmp_path / "icon_clock.bin").write_bytes(b"corrupt")
    app.dependency_overrides[get_settings] = lambda: Settings(
        DATABASE_URL="sqlite+pysqlite:///:memory:",
        LEOPOLD_ADMIN_PROVISIONING_TOKEN=ADMIN_TOKEN,
        LEOPOLD_ASSET_CATALOG_DIR=str(tmp_path),
    )
    response = client.get(
        "/v1/device/assets/icon_clock.bin",
        headers=device_headers(credentials),
    )
    assert response.status_code == 503


def test_admin_publishes_revision_two_and_device_receives_it(
    client, session_factory
) -> None:
    credentials = provision(client)
    response = client.put(
        f"/v1/admin/devices/{credentials['device_id']}/ui",
        headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
        json=replacement_screen(),
    )
    assert response.status_code == 200
    assert response.headers["etag"] == '"2"'
    assert response.json()["revision"] == 2
    assert response.json()["screens"][0]["id"] == "remote-home"

    with session_factory() as db:
        device = db.scalar(
            select(Device).where(Device.device_id == credentials["device_id"])
        )
        revisions = db.scalars(
            select(DeviceUIDocument)
            .where(DeviceUIDocument.device_id == device.id)
            .order_by(DeviceUIDocument.revision)
        ).all()
        assert [(item.revision, item.is_active) for item in revisions] == [
            (1, False),
            (2, True),
        ]
        assert sum(item.is_active for item in revisions) == 1

    current = client.get("/v1/device/ui", headers=device_headers(credentials))
    assert current.status_code == 200
    assert current.json()["revision"] == 2
    assert current.headers["etag"] == '"controls-v1-2"'
    stale_etag = client.get(
        "/v1/device/ui",
        headers={**device_headers(credentials), "If-None-Match": '"1"'},
    )
    assert stale_etag.status_code == 200
    assert stale_etag.json()["revision"] == 2


def test_publish_ui_accepts_document_without_revision(client) -> None:
    credentials = provision(client)
    response = client.put(
        f"/v1/admin/devices/{credentials['device_id']}/ui",
        headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
        json={
            "schema_version": 1,
            "poll_after_ms": 7000,
            "screens": [replacement_screen("full-document")],
        },
    )
    assert response.status_code == 200
    assert response.json()["revision"] == 2
    assert response.json()["poll_after_ms"] == 7000


def test_publish_ui_rejects_bad_token_unknown_device_and_invalid_document(
    client,
) -> None:
    credentials = provision(client)
    path = f"/v1/admin/devices/{credentials['device_id']}/ui"
    assert (
        client.put(
            path,
            headers={"Authorization": "Bearer incorrect"},
            json=replacement_screen(),
        ).status_code
        == 401
    )
    assert (
        client.put(
            "/v1/admin/devices/LPD-FFFFFF/ui",
            headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
            json=replacement_screen(),
        ).status_code
        == 404
    )
    invalid = replacement_screen()
    invalid["background"] = "black"
    assert (
        client.put(
            path,
            headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
            json=invalid,
        ).status_code
        == 422
    )


def test_concurrent_ui_updates_leave_exactly_one_active_revision(
    tmp_path,
) -> None:
    engine = create_engine(f"sqlite+pysqlite:///{tmp_path / 'concurrency.db'}")
    Base.metadata.create_all(engine)
    factory = sessionmaker(bind=engine, expire_on_commit=False)

    def override_db():
        with factory() as session:
            yield session

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_settings] = lambda: Settings(
        DATABASE_URL="sqlite+pysqlite:///:memory:",
        LEOPOLD_ADMIN_PROVISIONING_TOKEN=ADMIN_TOKEN,
    )
    try:
        with TestClient(app) as concurrent_client:
            credentials = provision(concurrent_client)
            path = f"/v1/admin/devices/{credentials['device_id']}/ui"
            start = Barrier(2)

            def update(screen_id: str) -> int:
                start.wait()
                return concurrent_client.put(
                    path,
                    headers={"Authorization": f"Bearer {ADMIN_TOKEN}"},
                    json=replacement_screen(screen_id),
                ).status_code

            with ThreadPoolExecutor(max_workers=2) as executor:
                statuses = list(executor.map(update, ["concurrent-a", "concurrent-b"]))

        assert all(code in {200, 409} for code in statuses)
        assert 200 in statuses
        with factory() as db:
            device = db.scalar(
                select(Device).where(Device.device_id == credentials["device_id"])
            )
            active_count = db.scalar(
                select(func.count())
                .select_from(DeviceUIDocument)
                .where(
                    DeviceUIDocument.device_id == device.id,
                    DeviceUIDocument.is_active.is_(True),
                )
            )
            assert active_count == 1
    finally:
        app.dependency_overrides.clear()
        Base.metadata.drop_all(engine)
        engine.dispose()


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


def test_computer_command_feed_is_authenticated_filtered_and_device_scoped(client) -> None:
    first = provision(client, "Mac controls")
    second = provision(client, "Other device")

    def send(credentials: dict, event_id: str, component_id: str) -> None:
        response = client.post(
            "/v1/device/events",
            headers=device_headers(credentials),
            json={
                "event_id": event_id,
                "revision": 1,
                "type": "tap",
                "component_id": component_id,
                "payload": {},
                "occurred_at": "2026-07-29T08:01:00Z",
            },
        )
        assert response.status_code == 202

    send(first, "evt-volume", "computer-volume-up")
    send(first, "evt-unrelated", "continue")
    send(second, "evt-other", "computer-screenshot")

    path = "/v1/device/computer-commands?after=2020-01-01T00:00:00Z"
    response = client.get(path, headers=device_headers(first))
    assert response.status_code == 200
    assert response.json() == [
        {
            "event_id": "evt-volume",
            "component_id": "computer-volume-up",
            "received_at": response.json()[0]["received_at"],
        }
    ]
    assert client.get(path).status_code == 401


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

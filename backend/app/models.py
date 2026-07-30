import enum
import uuid
from datetime import datetime, timezone

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Enum,
    ForeignKey,
    Index,
    Integer,
    String,
    UniqueConstraint,
    Uuid,
    text,
)
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


class Base(DeclarativeBase):
    pass


class DeviceStatus(str, enum.Enum):
    provisioned = "provisioned"
    online = "online"
    offline = "offline"
    disabled = "disabled"


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[uuid.UUID] = mapped_column(Uuid, primary_key=True, default=uuid.uuid4)
    device_id: Mapped[str] = mapped_column(String(32), unique=True, index=True)
    label: Mapped[str | None] = mapped_column(String(120))
    secret_hash: Mapped[str] = mapped_column(String(64))
    firmware_version: Mapped[str | None] = mapped_column(String(32))
    status: Mapped[DeviceStatus] = mapped_column(
        Enum(DeviceStatus, name="device_status"), default=DeviceStatus.provisioned
    )
    capabilities: Mapped[dict] = mapped_column(JSON, default=dict)
    last_ip: Mapped[str | None] = mapped_column(String(45))
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    wifi_rssi: Mapped[int | None] = mapped_column(Integer)
    battery_percent: Mapped[int | None] = mapped_column(Integer)
    uptime_seconds: Mapped[int | None] = mapped_column(Integer)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now, onupdate=utc_now
    )
    owner_id: Mapped[uuid.UUID | None] = mapped_column(
        ForeignKey("users.id", ondelete="SET NULL"), index=True
    )

    ui_documents: Mapped[list["DeviceUIDocument"]] = relationship(
        back_populates="device", cascade="all, delete-orphan"
    )
    owner: Mapped["User | None"] = relationship(back_populates="devices")
    claim_tokens: Mapped[list["DeviceClaimToken"]] = relationship(
        back_populates="device", cascade="all, delete-orphan"
    )


class DeviceUIDocument(Base):
    __tablename__ = "device_ui_documents"
    __table_args__ = (
        UniqueConstraint("device_id", "revision", name="uq_device_ui_revision"),
        Index(
            "uq_device_ui_active",
            "device_id",
            unique=True,
            postgresql_where=text("is_active"),
            sqlite_where=text("is_active = 1"),
        ),
    )

    id: Mapped[uuid.UUID] = mapped_column(Uuid, primary_key=True, default=uuid.uuid4)
    device_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), index=True
    )
    revision: Mapped[int] = mapped_column(Integer)
    document: Mapped[dict] = mapped_column(JSON)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now
    )

    device: Mapped[Device] = relationship(back_populates="ui_documents")


class DeviceEvent(Base):
    __tablename__ = "device_events"
    __table_args__ = (
        UniqueConstraint("device_id", "event_id", name="uq_device_event_id"),
    )

    id: Mapped[uuid.UUID] = mapped_column(Uuid, primary_key=True, default=uuid.uuid4)
    device_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), index=True
    )
    event_id: Mapped[str] = mapped_column(String(128))
    revision: Mapped[int] = mapped_column(Integer)
    type: Mapped[str] = mapped_column(String(32))
    component_id: Mapped[str] = mapped_column(String(64))
    payload: Mapped[dict] = mapped_column(JSON)
    occurred_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now
    )


class User(Base):
    __tablename__ = "users"

    id: Mapped[uuid.UUID] = mapped_column(Uuid, primary_key=True, default=uuid.uuid4)
    email: Mapped[str] = mapped_column(String(254), unique=True, index=True)
    display_name: Mapped[str] = mapped_column(String(80))
    password_hash: Mapped[str] = mapped_column(String(256))
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now
    )
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now, onupdate=utc_now
    )

    devices: Mapped[list[Device]] = relationship(back_populates="owner")
    sessions: Mapped[list["UserSession"]] = relationship(
        back_populates="user", cascade="all, delete-orphan"
    )


class UserSession(Base):
    __tablename__ = "user_sessions"

    id: Mapped[uuid.UUID] = mapped_column(Uuid, primary_key=True, default=uuid.uuid4)
    user_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("users.id", ondelete="CASCADE"), index=True
    )
    token_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now
    )

    user: Mapped[User] = relationship(back_populates="sessions")


class DeviceClaimToken(Base):
    __tablename__ = "device_claim_tokens"

    id: Mapped[uuid.UUID] = mapped_column(Uuid, primary_key=True, default=uuid.uuid4)
    device_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), index=True
    )
    token_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    used_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utc_now
    )

    device: Mapped[Device] = relationship(back_populates="claim_tokens")

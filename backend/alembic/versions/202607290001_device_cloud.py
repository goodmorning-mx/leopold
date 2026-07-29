"""Add Leopold Device Cloud tables.

Revision ID: 202607290001
Revises: 202607150001
Create Date: 2026-07-29 00:00:00.000000
"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "202607290001"
down_revision: str | None = "202607150001"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None

device_status = sa.Enum(
    "provisioned", "online", "offline", "disabled", name="device_status"
)


def upgrade() -> None:
    op.create_table(
        "devices",
        sa.Column("id", sa.Uuid(), nullable=False),
        sa.Column("device_id", sa.String(length=32), nullable=False),
        sa.Column("label", sa.String(length=120), nullable=True),
        sa.Column("secret_hash", sa.String(length=64), nullable=False),
        sa.Column("firmware_version", sa.String(length=32), nullable=True),
        sa.Column("status", device_status, nullable=False),
        sa.Column("capabilities", sa.JSON(), nullable=False),
        sa.Column("last_ip", sa.String(length=45), nullable=True),
        sa.Column("last_seen_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("wifi_rssi", sa.Integer(), nullable=True),
        sa.Column("battery_percent", sa.Integer(), nullable=True),
        sa.Column("uptime_seconds", sa.Integer(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("device_id"),
    )
    op.create_index("ix_devices_device_id", "devices", ["device_id"], unique=True)

    op.create_table(
        "device_ui_documents",
        sa.Column("id", sa.Uuid(), nullable=False),
        sa.Column("device_id", sa.Uuid(), nullable=False),
        sa.Column("revision", sa.Integer(), nullable=False),
        sa.Column("document", sa.JSON(), nullable=False),
        sa.Column("is_active", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.ForeignKeyConstraint(["device_id"], ["devices.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("device_id", "revision", name="uq_device_ui_revision"),
    )
    op.create_index(
        "ix_device_ui_documents_device_id",
        "device_ui_documents",
        ["device_id"],
    )
    op.create_index(
        "uq_device_ui_active",
        "device_ui_documents",
        ["device_id"],
        unique=True,
        postgresql_where=sa.text("is_active"),
    )

    op.create_table(
        "device_events",
        sa.Column("id", sa.Uuid(), nullable=False),
        sa.Column("device_id", sa.Uuid(), nullable=False),
        sa.Column("event_id", sa.String(length=128), nullable=False),
        sa.Column("revision", sa.Integer(), nullable=False),
        sa.Column("type", sa.String(length=32), nullable=False),
        sa.Column("component_id", sa.String(length=64), nullable=False),
        sa.Column("payload", sa.JSON(), nullable=False),
        sa.Column("occurred_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("received_at", sa.DateTime(timezone=True), nullable=False),
        sa.ForeignKeyConstraint(["device_id"], ["devices.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("device_id", "event_id", name="uq_device_event_id"),
    )
    op.create_index("ix_device_events_device_id", "device_events", ["device_id"])


def downgrade() -> None:
    op.drop_index("ix_device_events_device_id", table_name="device_events")
    op.drop_table("device_events")
    op.drop_index("uq_device_ui_active", table_name="device_ui_documents")
    op.drop_index("ix_device_ui_documents_device_id", table_name="device_ui_documents")
    op.drop_table("device_ui_documents")
    op.drop_index("ix_devices_device_id", table_name="devices")
    op.drop_table("devices")
    device_status.drop(op.get_bind(), checkfirst=True)

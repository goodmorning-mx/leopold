"""Add signed firmware releases, assignments and installation history."""
from collections.abc import Sequence
import sqlalchemy as sa
from alembic import op

revision: str = "202610210001"
down_revision: str | None = "202608030001"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None

def upgrade() -> None:
    op.create_table("firmware_releases",
        sa.Column("id", sa.String(64), primary_key=True),
        sa.Column("manifest", sa.JSON(), nullable=False),
        sa.Column("signature", sa.String(128), nullable=False),
        sa.Column("artifact_path", sa.String(512), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False))
    op.create_table("firmware_assignments",
        sa.Column("device_id", sa.Uuid(), sa.ForeignKey("devices.id", ondelete="CASCADE"), primary_key=True),
        sa.Column("release_id", sa.String(64), sa.ForeignKey("firmware_releases.id"), nullable=True),
        sa.Column("revoked", sa.Boolean(), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False))
    op.create_table("firmware_installations",
        sa.Column("id", sa.Uuid(), primary_key=True),
        sa.Column("device_id", sa.Uuid(), sa.ForeignKey("devices.id", ondelete="CASCADE"), nullable=False),
        sa.Column("event_id", sa.String(128), nullable=False),
        sa.Column("release_id", sa.String(64), sa.ForeignKey("firmware_releases.id"), nullable=False),
        sa.Column("state", sa.String(32), nullable=False),
        sa.Column("detail", sa.String(256), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.UniqueConstraint("device_id", "event_id", name="uq_firmware_installation_event"))
    op.create_index("ix_firmware_installations_device_id", "firmware_installations", ["device_id"])
    op.create_index("ix_firmware_installations_release_id", "firmware_installations", ["release_id"])

def downgrade() -> None:
    op.drop_table("firmware_installations")
    op.drop_table("firmware_assignments")
    op.drop_table("firmware_releases")

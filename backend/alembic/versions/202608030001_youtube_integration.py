"""Add YouTube OAuth connections.

Revision ID: 202608030001
Revises: 202607300002
"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "202608030001"
down_revision: str | None = "202607300002"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "youtube_connections",
        sa.Column("id", sa.Uuid(), nullable=False),
        sa.Column("user_id", sa.Uuid(), nullable=False),
        sa.Column("channel_id", sa.String(length=128), nullable=False),
        sa.Column("display_name", sa.String(length=160), nullable=True),
        sa.Column("access_token_encrypted", sa.String(length=4096), nullable=False),
        sa.Column("refresh_token_encrypted", sa.String(length=4096), nullable=False),
        sa.Column("token_expires_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("scope", sa.String(length=512), nullable=False),
        sa.Column("cached_feed", sa.JSON(), nullable=False),
        sa.Column("feed_updated_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("active", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False),
        sa.ForeignKeyConstraint(["user_id"], ["users.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("user_id"),
    )
    op.create_index(
        "ix_youtube_connections_user_id", "youtube_connections", ["user_id"], unique=True
    )
    op.create_table(
        "youtube_oauth_states",
        sa.Column("id", sa.Uuid(), nullable=False),
        sa.Column("user_id", sa.Uuid(), nullable=False),
        sa.Column("token_hash", sa.String(length=64), nullable=False),
        sa.Column("expires_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.ForeignKeyConstraint(["user_id"], ["users.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index("ix_youtube_oauth_states_user_id", "youtube_oauth_states", ["user_id"])
    op.create_index(
        "ix_youtube_oauth_states_token_hash", "youtube_oauth_states", ["token_hash"], unique=True
    )
    op.create_index("ix_youtube_oauth_states_expires_at", "youtube_oauth_states", ["expires_at"])


def downgrade() -> None:
    op.drop_index("ix_youtube_oauth_states_expires_at", table_name="youtube_oauth_states")
    op.drop_index("ix_youtube_oauth_states_token_hash", table_name="youtube_oauth_states")
    op.drop_index("ix_youtube_oauth_states_user_id", table_name="youtube_oauth_states")
    op.drop_table("youtube_oauth_states")
    op.drop_index("ix_youtube_connections_user_id", table_name="youtube_connections")
    op.drop_table("youtube_connections")

from functools import lru_cache

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """Application settings loaded from environment variables."""

    app_name: str = Field(default="GoodMorning MVP Template API", alias="APP_NAME")
    app_env: str = Field(default="local", alias="APP_ENV")
    cors_origins: str = Field(default="http://localhost:5173", alias="CORS_ORIGINS")
    database_url: str = Field(
        default="postgresql+psycopg://goodmorning:goodmorning_local_password@localhost:5432/goodmorning_mvp",
        alias="DATABASE_URL",
    )
    admin_provisioning_token: str | None = Field(
        default=None, alias="LEOPOLD_ADMIN_PROVISIONING_TOKEN"
    )
    device_offline_after_seconds: int = Field(
        default=90, ge=30, le=3600, alias="LEOPOLD_DEVICE_OFFLINE_AFTER_SECONDS"
    )
    ui_poll_interval_seconds: int = Field(
        default=5, ge=1, le=300, alias="LEOPOLD_UI_POLL_INTERVAL_SECONDS"
    )
    heartbeat_interval_seconds: int = Field(
        default=30, ge=5, le=3600, alias="LEOPOLD_HEARTBEAT_INTERVAL_SECONDS"
    )
    spotify_client_id: str | None = Field(default=None, alias="SPOTIFY_CLIENT_ID")
    spotify_client_secret: str | None = Field(
        default=None, alias="SPOTIFY_CLIENT_SECRET"
    )
    spotify_redirect_uri: str = Field(
        default="http://localhost:8000/v1/integrations/spotify/callback",
        alias="SPOTIFY_REDIRECT_URI",
    )
    youtube_client_id: str | None = Field(default=None, alias="YOUTUBE_CLIENT_ID")
    youtube_client_secret: str | None = Field(
        default=None, alias="YOUTUBE_CLIENT_SECRET"
    )
    youtube_redirect_uri: str = Field(
        default="http://localhost:8000/v1/integrations/youtube/callback",
        alias="YOUTUBE_REDIRECT_URI",
    )
    integration_encryption_key: str | None = Field(
        default=None, alias="LEOPOLD_INTEGRATION_ENCRYPTION_KEY"
    )

    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    @property
    def allowed_origins(self) -> list[str]:
        return [
            origin.strip() for origin in self.cors_origins.split(",") if origin.strip()
        ]


@lru_cache
def get_settings() -> Settings:
    return Settings()

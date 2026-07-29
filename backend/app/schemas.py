import json
from datetime import datetime
from typing import Annotated, Literal, Union

from pydantic import BaseModel, ConfigDict, Field, field_validator

Color = Annotated[str, Field(pattern=r"^#[0-9A-Fa-f]{6}$")]
Coordinate = Annotated[int, Field(ge=0, le=465)]


class ComponentBase(BaseModel):
    model_config = ConfigDict(extra="ignore")

    id: str = Field(min_length=1, max_length=64, pattern=r"^[A-Za-z0-9_-]+$")
    x: Coordinate
    y: Coordinate


class TextComponent(ComponentBase):
    type: Literal["text"]
    text: str = Field(min_length=1, max_length=256)
    align: Literal["left", "center", "right"] = "left"
    size: int = Field(ge=1, le=8)
    color: Color


class IndicatorComponent(ComponentBase):
    type: Literal["indicator"]
    label: str = Field(min_length=1, max_length=80)
    state: str = Field(min_length=1, max_length=32)


class ButtonComponent(ComponentBase):
    type: Literal["button"]
    label: str = Field(min_length=1, max_length=80)
    width: int = Field(gt=0, le=466)
    height: int = Field(gt=0, le=466)


class ProgressComponent(ComponentBase):
    type: Literal["progress"]
    value: int = Field(ge=0, le=100)
    width: int = Field(gt=0, le=466)
    color: Color | None = None


Component = Annotated[
    Union[TextComponent, IndicatorComponent, ButtonComponent, ProgressComponent],
    Field(discriminator="type"),
]


class Screen(BaseModel):
    model_config = ConfigDict(extra="ignore")

    id: str = Field(min_length=1, max_length=64)
    background: Color
    components: list[Component] = Field(max_length=32)


class UIDocument(BaseModel):
    model_config = ConfigDict(extra="ignore")

    schema_version: Literal[1]
    revision: int = Field(ge=1)
    poll_after_ms: int = Field(ge=1000, le=300000)
    screen: Screen


class UIPublishDocument(BaseModel):
    """SDUI document accepted from admins; the server owns its revision."""

    model_config = ConfigDict(extra="ignore")

    schema_version: Literal[1]
    revision: None = None
    poll_after_ms: int = Field(ge=1000, le=300000)
    screen: Screen


class ProvisionDeviceRequest(BaseModel):
    label: str | None = Field(default=None, min_length=1, max_length=120)


class ProvisionDeviceResponse(BaseModel):
    device_id: str
    device_secret: str


class ConnectRequest(BaseModel):
    firmware_version: str = Field(min_length=1, max_length=32)
    capabilities: dict[str, bool | str | int] = Field(
        default_factory=dict, max_length=16
    )

    @field_validator("capabilities")
    @classmethod
    def capabilities_are_small(cls, value: dict) -> dict:
        if len(json.dumps(value, separators=(",", ":")).encode()) > 4096:
            raise ValueError("capabilities must not exceed 4096 bytes")
        return value


class ConnectResponse(BaseModel):
    device_id: str
    server_time: datetime
    heartbeat_interval_seconds: int
    ui_poll_interval_seconds: int
    ui_revision: int


class DeviceEventRequest(BaseModel):
    event_id: str = Field(min_length=1, max_length=128)
    revision: int = Field(ge=1)
    type: Literal["tap"]
    component_id: str = Field(min_length=1, max_length=64)
    payload: dict = Field(default_factory=dict)
    occurred_at: datetime

    @field_validator("payload")
    @classmethod
    def payload_is_small(cls, value: dict) -> dict:
        if len(json.dumps(value, separators=(",", ":"), default=str).encode()) > 4096:
            raise ValueError("payload must not exceed 4096 bytes")
        return value


class HeartbeatRequest(BaseModel):
    firmware_version: str = Field(min_length=1, max_length=32)
    wifi_rssi: int = Field(ge=-127, le=0)
    battery_percent: int = Field(ge=0, le=100)
    uptime_seconds: int = Field(ge=0, le=2_147_483_647)


class HeartbeatResponse(BaseModel):
    ok: Literal[True]
    server_time: datetime
    ui_revision: int

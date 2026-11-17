import os

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware

from app.config import get_settings
from app.auth_api import router as auth_router
from app.device_api import router as device_router
from app.spotify_api import router as spotify_router
from app.youtube_api import router as youtube_router
from app.weather_api import router as weather_router
from app.firmware_api import router as firmware_router

settings = get_settings()

app = FastAPI(title=settings.app_name)
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.allowed_origins,
    allow_credentials=True,
    allow_methods=["GET", "POST", "PUT", "DELETE", "OPTIONS"],
    allow_headers=["*"],
)
app.include_router(device_router)
app.include_router(auth_router)
app.include_router(spotify_router)
app.include_router(youtube_router)
app.include_router(weather_router)
app.include_router(firmware_router)


@app.middleware("http")
async def reject_large_device_payloads(request: Request, call_next):
    """Reject declared device payloads above the deliberately small MVP limit."""

    release_upload = request.url.path.startswith("/v1/admin/firmware/releases/")
    if request.url.path.startswith("/v1/") and not release_upload:
        content_length = request.headers.get("content-length")
        if content_length and int(content_length) > 16_384:
            return JSONResponse(
                status_code=413, content={"detail": "Payload too large"}
            )
    return await call_next(request)


@app.middleware("http")
async def local_ota_acknowledgement_loss(request: Request, call_next):
    """Withhold one committed healthy-status response in an explicit local run."""

    if (
        os.environ.get("APP_ENV") != "local"
        or os.environ.get("LEOPOLD_LOCAL_OTA_DROP_FIRST_HEALTHY_ACK") != "1"
        or request.method != "POST"
        or request.url.path != "/v1/device/firmware/status"
        or getattr(app.state, "local_ota_healthy_ack_failed", False)
    ):
        return await call_next(request)

    try:
        payload = await request.json()
    except (ValueError, UnicodeDecodeError):
        return await call_next(request)

    response = await call_next(request)
    if (
        isinstance(payload, dict)
        and payload.get("state") == "healthy"
        and 200 <= response.status_code < 300
    ):
        # The real endpoint commits first; the device retry must reuse its event ID.
        app.state.local_ota_healthy_ack_failed = True
        return JSONResponse(
            status_code=503,
            content={"detail": "local OTA test withheld one healthy-status acknowledgement"},
        )
    return response


@app.get("/health")
def health() -> dict[str, str]:
    return {
        "service": settings.app_name,
        "status": "ok",
        "environment": settings.app_env,
    }

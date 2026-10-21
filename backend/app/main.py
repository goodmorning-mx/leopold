from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware

from app.config import get_settings
from app.auth_api import router as auth_router
from app.device_api import router as device_router
from app.spotify_api import router as spotify_router
from app.youtube_api import router as youtube_router
from app.weather_api import router as weather_router

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


@app.middleware("http")
async def reject_large_device_payloads(request: Request, call_next):
    """Reject declared device payloads above the deliberately small MVP limit."""

    if request.url.path.startswith("/v1/"):
        content_length = request.headers.get("content-length")
        if content_length and int(content_length) > 16_384:
            return JSONResponse(
                status_code=413, content={"detail": "Payload too large"}
            )
    return await call_next(request)


@app.get("/health")
def health() -> dict[str, str]:
    return {
        "service": settings.app_name,
        "status": "ok",
        "environment": settings.app_env,
    }

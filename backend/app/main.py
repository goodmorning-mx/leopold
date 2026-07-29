from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware

from app.config import get_settings
from app.device_api import router as device_router

settings = get_settings()

app = FastAPI(title=settings.app_name)
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.allowed_origins,
    allow_credentials=False,
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["*"],
)
app.include_router(device_router)


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

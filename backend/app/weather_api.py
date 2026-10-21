"""Cloud-backed postal-code lookup for the supported US and Mexico regions."""

from __future__ import annotations

import re

import httpx
from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel

from app.config import get_settings

router = APIRouter(prefix="/v1/weather", tags=["weather"])

POSTAL_PATTERNS = {
    "US": re.compile(r"^[0-9]{5}(?:-[0-9]{4})?$"),
    "MX": re.compile(r"^[0-9]{5}$"),
}


class WeatherLocation(BaseModel):
    country: str
    postal_code: str
    name: str
    admin1: str | None = None
    latitude: float
    longitude: float
    timezone: str | None = None
    provider: str = "Open-Meteo Geocoding API"


@router.get("/geocode", response_model=WeatherLocation)
async def geocode_postal_code(
    country: str = Query(pattern=r"^(US|MX)$"),
    postal_code: str = Query(min_length=1, max_length=10),
) -> WeatherLocation:
    normalized_country = country.upper()
    code = postal_code.strip()
    if normalized_country not in POSTAL_PATTERNS:
        raise HTTPException(status_code=422, detail="Select the United States or Mexico.")
    pattern = POSTAL_PATTERNS[normalized_country]
    if not pattern.fullmatch(code):
        raise HTTPException(status_code=422, detail="Postal code format is not valid for the selected country.")

    settings = get_settings()
    provider_code = code[:5] if normalized_country == "US" and len(code) == 10 else code
    params: dict[str, str | int] = {"name": provider_code, "countryCode": normalized_country, "count": 10}
    if settings.weather_geocoding_api_key:
        params["apikey"] = settings.weather_geocoding_api_key

    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            response = await client.get(settings.weather_geocoding_url, params=params)
            response.raise_for_status()
            payload = response.json()
    except (httpx.HTTPError, ValueError) as exc:
        raise HTTPException(status_code=503, detail="Postal-code lookup is temporarily unavailable.") from exc

    results = payload.get("results", []) if isinstance(payload, dict) else []
    for result in results:
        if not isinstance(result, dict) or result.get("country_code", "").upper() != normalized_country:
            continue
        postcodes = result.get("postcodes", [])
        if provider_code not in postcodes:
            continue
        try:
            return WeatherLocation(
                country=normalized_country,
                postal_code=code,
                name=str(result["name"]),
                admin1=str(result["admin1"]) if result.get("admin1") else None,
                latitude=float(result["latitude"]),
                longitude=float(result["longitude"]),
                timezone=str(result["timezone"]) if result.get("timezone") else None,
            )
        except (KeyError, TypeError, ValueError) as exc:
            raise HTTPException(status_code=502, detail="The location service returned an incomplete result.") from exc
    raise HTTPException(status_code=404, detail="No exact match was found for that postal code.")

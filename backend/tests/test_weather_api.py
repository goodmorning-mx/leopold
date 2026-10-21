import asyncio

import httpx
import pytest
from fastapi import HTTPException

from app import weather_api


class MockResponse:
    def __init__(self, payload, status_code=200):
        self.payload = payload
        self.status_code = status_code

    def raise_for_status(self):
        if self.status_code >= 400:
            raise httpx.HTTPStatusError("failed", request=None, response=None)

    def json(self):
        return self.payload


def install_client(monkeypatch, payload):
    class MockClient:
        def __init__(self, **kwargs):
            self.kwargs = kwargs

        async def __aenter__(self):
            return self

        async def __aexit__(self, *args):
            return None

        async def get(self, url, params):
            assert params["countryCode"] in {"US", "MX"}
            assert params["count"] == 10
            return MockResponse(payload)

    monkeypatch.setattr(weather_api.httpx, "AsyncClient", MockClient)


def test_us_zip_lookup_preserves_leading_zero(monkeypatch):
    install_client(monkeypatch, {"results": [{
        "country_code": "US", "postcodes": ["00501"], "name": "Holtsville",
        "admin1": "New York", "latitude": 40.8154, "longitude": -73.0451,
        "timezone": "America/New_York",
    }]})
    result = asyncio.run(weather_api.geocode_postal_code("US", "00501"))
    assert result.postal_code == "00501"
    assert result.country == "US"
    assert result.name == "Holtsville"


def test_us_zip_plus_four_uses_base_postcode_and_preserves_entry(monkeypatch):
    install_client(monkeypatch, {"results": [{
        "country_code": "US", "postcodes": ["02139"], "name": "Cambridge",
        "latitude": 42.36, "longitude": -71.10,
    }]})
    result = asyncio.run(weather_api.geocode_postal_code("US", "02139-1234"))
    assert result.postal_code == "02139-1234"


def test_mexico_postal_lookup_filters_country(monkeypatch):
    install_client(monkeypatch, {"results": [{
        "country_code": "MX", "postcodes": ["06000"], "name": "Cuauhtémoc",
        "latitude": 19.4326, "longitude": -99.1332,
    }]})
    result = asyncio.run(weather_api.geocode_postal_code("MX", "06000"))
    assert result.country == "MX"
    assert result.postal_code == "06000"


@pytest.mark.parametrize(("country", "postal"), [("US", "123"), ("MX", "1234"), ("CA", "12345")])
def test_rejects_unsupported_or_malformed_postal_code(country, postal):
    with pytest.raises(HTTPException) as error:
        asyncio.run(weather_api.geocode_postal_code(country, postal))
    assert error.value.status_code == 422


def test_lookup_requires_exact_postcode_match(monkeypatch):
    install_client(monkeypatch, {"results": [{
        "country_code": "US", "postcodes": ["12346"], "name": "Nearby", "latitude": 1, "longitude": 2,
    }]})
    with pytest.raises(HTTPException) as error:
        asyncio.run(weather_api.geocode_postal_code("US", "12345"))
    assert error.value.status_code == 404

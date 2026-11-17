# Architecture

Leopold uses a React/Vite frontend, a FastAPI backend and PostgreSQL. Local Compose defines three services:

- `frontend`: the product app, onboarding and browser simulator.
- `backend`: device and user authentication, onboarding, SDUI, telemetry, cloud integrations, asset catalog, geocoding and signed firmware release endpoints.
- `postgres`: PostgreSQL 16 for product data, with schema managed by Alembic.

The backend stores Leopold devices, one active versioned SDUI document per
device, idempotent device events, and basic heartbeat telemetry in PostgreSQL.
Device authentication uses a public device ID plus a one-time secret whose
SHA-256 digest is the only persisted representation. The HTTPS Device Cloud
contract is documented in `docs/device-api.md`.

Runtime configuration is supplied through environment variables documented in `.env.example`.

## Firmware and simulator

The firmware draws the generated offline Home before optional storage and
network services start. The main application coordinates touch, navigation and
committed UI state. Cloud and weather workers pass results to that application.
The ESP32 and WASM compile the same [LVGL renderer](../shared/lvgl_renderer.cpp)
and consume the same [UI and asset contracts](../shared/).

See the [portable build](firmware-build.md), [simulator](simulator.md) and
[hardware interfaces](hardware.md). The imported firmware, renderer and
simulator baseline is Leopold source commit
`82c23dc165bdf08d30ebb1ea854c28aea34be174`.

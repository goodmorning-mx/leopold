# Architecture

The template contains three services:

- `frontend`: a React/Vite single-page app that calls the backend health endpoint.
- `backend`: a FastAPI API exposing `GET /health` and preparing PostgreSQL connectivity.
- `db`: PostgreSQL for product data when a product adds its own schema.

The backend stores Leopold devices, one active versioned SDUI document per
device, idempotent device events, and basic heartbeat telemetry in PostgreSQL.
Device authentication uses a public device ID plus a one-time secret whose
SHA-256 digest is the only persisted representation. The HTTPS Device Cloud
contract is documented in `docs/device-api.md`.

Runtime configuration is supplied through environment variables documented in `.env.example`.

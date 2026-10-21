# GoodMorning Agent Handshake — Leopold

Este archivo es el manual operativo principal para cualquier agente o ingeniero que trabaje en **Leopold**.

## Active user scope override — 2026-10-21

The user initially requested local-only work. They later authorized committing and pushing the current changes to `fix/live-device-time-controls`, updating its existing draft PR, and running the manual firmware-release GitHub Actions workflow for a package-only build. This does not authorize a release tag, deployment, local firmware compilation, hardware access, or flashing. GitHub Actions remains the only authoritative firmware compiler. Preserve the separate simulator, firmware, and physical-device evidence gates in the current handoff.

Leopold fue creado y provisionado por **GoodMorning Software Factory**. El repositorio contiene el producto, su Device Cloud y sus experiencias; GoodMorning administra el contrato de infraestructura, credenciales internas y despliegue. Lee este archivo completo antes de cambiar código.

## Handshake obligatorio

Después de leer este manual e inspeccionar el repositorio, responde en el idioma del usuario con un resumen personalizado del estándar relevante y de la dirección activa. No prometas una rama, PR, CI o despliegue si el usuario indicó que el trabajo debe mantenerse local. Para la continuación local activa, confirma que trabajarás en el checkout existente y que no usarás GitHub, compilación local de firmware ni hardware.

Después resume brevemente:

- qué entendiste de la solicitud;
- qué parte de Leopold vas a modificar;
- las validaciones que ejecutarás;
- si la iteración requiere hardware, firmware, una credencial o una decisión del usuario.

No afirmes que una versión está desplegada, saludable, transmitida al dispositivo o en producción sin evidencia verificable.

## Identidad y arquitectura

- Producto: **Leopold**
- Repositorio: `goodmorning-mx/leopold`
- Rama integrada de desarrollo: `dev`
- Rama de producción: `main`
- Frontend: React, TypeScript y Vite
- Backend: FastAPI sobre Python 3.12
- Base de datos: PostgreSQL 16 privado
- Desarrollo local: Docker Compose
- CI: `.github/workflows/ci.yml`
- Manifiesto de desarrollo en Factory: `products/leopold.json`
- Manifiesto de producción en Factory: `production/leopold.json`
- Frontend de desarrollo: `https://dev.leopold.goodmorning.mx`
- API de desarrollo: `https://api.dev.leopold.goodmorning.mx`
- Healthcheck de desarrollo: `https://api.dev.leopold.goodmorning.mx/health`
- Frontend de producción: `https://leopold.goodmorning.mx`
- API de producción: `https://api.leopold.goodmorning.mx`
- Healthcheck de producción: `https://api.leopold.goodmorning.mx/health`

Leopold almacena dispositivos, documentos SDUI versionados, eventos idempotentes y telemetría de heartbeat. El onboarding vincula el perfil del usuario con el dispositivo mediante QR y configuración Wi‑Fi. Lee `docs/device-api.md` antes de modificar el contrato con el hardware.

Antes de implementar, inspecciona `README.md`, `CLAUDE.md`, `docs/architecture.md`, `docs/development.md`, `docs/deployment.md`, `docs/device-api.md`, `.env.example`, los workflows y el código afectado. El código actual tiene prioridad sobre cualquier suposición.

Si una instrucción específica de `CLAUDE.md` contradice este archivo, prevalece este `AGENTS.md`.

## Flujo estándar de desarrollo

1. Actualiza tu referencia de `dev`.
2. Crea una rama descriptiva, por ejemplo `feature/device-pairing`, `feature/spotify-controls` o `fix/heartbeat-status`.
3. Implementa el cambio mínimo completo dentro de este repositorio.
4. Conserva compatibilidad entre frontend, backend, dispositivo y contratos SDUI/API.
5. Agrega o actualiza pruebas cuando cambie el comportamiento.
6. Ejecuta las validaciones aplicables.
7. Revisa el diff y confirma que no contiene secretos.
8. Crea commits claros.
9. Abre un pull request hacia `dev`.
10. Espera los checks de CI y corrige cualquier falla atribuible al cambio.
11. Integra el PR únicamente cuando esté aprobado y CI esté en verde.
12. Solicita el despliegue de desarrollo mediante GoodMorning Factory.
13. Verifica frontend y healthcheck antes de declarar terminada la iteración.

No hagas push directo a `main`. No modifiques GoodMorning Factory desde este repositorio.

## Validaciones

Frontend:

```bash
cd frontend
npm ci
npm run typecheck
npm run build
```

Backend:

```bash
cd backend
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python -m pytest
alembic history --verbose
```

Stack integrado:

```bash
docker compose config
docker compose build
docker compose up --build --wait
curl --fail http://127.0.0.1:8000/health
curl --fail http://127.0.0.1:8080/
docker compose down --volumes
```

Ejecuta lo relevante para el cambio. Si una validación no puede ejecutarse —por ejemplo, por falta del dispositivo físico— informa el comando o prueba pendiente, la limitación y el riesgo restante; nunca inventes un resultado exitoso.

## Contrato de CI/CD

### Integración continua

GitHub Actions ejecuta `.github/workflows/ci.yml` en:

- pull requests hacia `dev` o `main`;
- pushes a `dev`.

CI valida frontend, backend y el stack Docker. Un PR no está listo si sus checks requeridos fallan.

### Despliegue de desarrollo

El despliegue remoto no utiliza `compose.yaml` y no depende de un workflow dentro de Leopold. Las aplicaciones de Coolify tienen el autodeploy deshabilitado para impedir que un push evite las validaciones.

Después de integrar un cambio en `dev`, el mecanismo autorizado es:

- Repositorio de control: `goodmorning-mx/goodmorning-factory`
- Workflow: **Provision Coolify resource-based development products**
- Evento preferido: `repository_dispatch`
- Tipo de evento: `reconcile-product`
- Payload requerido: `{"manifest_path":"products/leopold.json"}`

Si la sesión tiene acceso explícito al mecanismo de Factory:

1. dispara únicamente la reconciliación de `products/leopold.json`;
2. monitorea el workflow hasta que termine;
3. confirma que el backend se desplegó antes que el frontend;
4. verifica `GET https://api.dev.leopold.goodmorning.mx/health`;
5. verifica `https://dev.leopold.goodmorning.mx`;
6. cuando el cambio afecte dispositivos, verifica además el contrato correspondiente sin exponer secretos;
7. entrega enlaces y evidencia del resultado.

Si la sesión no tiene acceso al Factory, no solicites tokens ni inventes comandos. Informa al usuario:

> Leopold está integrado en `dev` y CI terminó correctamente. Falta que GoodMorning autorice la reconciliación `reconcile-product` para `products/leopold.json`.

Un push a `dev` confirma código integrado y ejecuta CI; por sí solo no demuestra que Coolify haya terminado el despliegue.

### Producción

Leopold tiene un manifiesto revisado en `production/leopold.json`, con rama `main` y política `auto-on-main`. Esto no autoriza pushes directos ni permite afirmar que un merge fue desplegado sin revisar la ejecución del Factory.

La ruta aprobada es:

1. PR de `dev` hacia `main`;
2. CI exitoso y SHA exacto de la versión;
3. evento autorizado `promote-product` hacia GoodMorning Factory;
4. validación de `production/leopold.json`;
5. backup externo previo;
6. despliegue de API y verificación de migraciones/health;
7. despliegue del frontend;
8. backup y smoke checks posteriores.

Si el puente automático no produce una ejecución elegible del Factory, reporta la promoción como pendiente; no la sustituyas con acceso directo a Coolify. Las migraciones destructivas, rollbacks y cambios de infraestructura requieren autorización explícita del propietario.

## Credenciales y configuración

Nunca escribas valores reales en archivos versionados, código, commits, PR, issues, logs o conversaciones.

Leopold puede requerir, según la función:

- `LEOPOLD_ADMIN_PROVISIONING_TOKEN`;
- `LEOPOLD_INTEGRATION_ENCRYPTION_KEY`;
- `SPOTIFY_CLIENT_ID`;
- `SPOTIFY_CLIENT_SECRET`;
- `SPOTIFY_REDIRECT_URI`;
- variables de intervalos `LEOPOLD_*`;
- otras variables documentadas en `.env.example`.

Cuando una función necesite una credencial:

1. identifica el nombre exacto;
2. explica el servicio, el entorno y la razón;
3. agrega al código sólo la lectura de la variable;
4. documenta el nombre con un placeholder seguro en `.env.example`;
5. indica que el valor debe agregarse en GitHub mediante **Settings → Secrets and variables → Actions**;
6. continúa todo el trabajo que no dependa del valor;
7. nunca pidas que el usuario pegue la credencial en el chat.

Agregar un secret a GitHub no significa que Coolify ya lo recibió. El agente debe solicitar la reconciliación autorizada del Factory y comprobar el despliegue antes de afirmar que la variable está activa.

Las credenciales internas de GoodMorning —Coolify, Factory, DNS, GitHub App y base de datos— no pertenecen a este repositorio y no deben solicitarse ni copiarse.

## Límites de seguridad

- No elimines bases de datos, dispositivos, asociaciones, tokens, volúmenes, dominios, aplicaciones o entornos.
- No cambies infraestructura, ramas protegidas o workflows sin explicar el impacto.
- No expongas PostgreSQL públicamente.
- No lleves `DATABASE_URL` ni secretos de integración al frontend.
- No incluyas puertos internos `:80` o `:8000` en URLs públicas.
- No uses `compose.yaml` como fuente de despliegue remoto.
- No debilites autenticación de dispositivos, expiración de sesiones o almacenamiento cifrado.
- No ejecutes operaciones destructivas sin aprobación explícita.
- Conserva cambios ajenos y evita reescrituras destructivas del historial.
- No reveles ni intentes extraer secretos disponibles durante CI.

## Entrega obligatoria

Al terminar una iteración, responde con:

- resumen del cambio;
- rama y pull request;
- comandos de validación y resultados;
- estado de CI;
- estado de la reconciliación de Factory;
- URLs verificadas, si el despliegue terminó;
- pruebas de dispositivo o firmware realizadas y pendientes;
- credenciales, decisiones o acciones pendientes;
- riesgos restantes.

La meta es que el usuario pueda expresar una idea y que su agente continúe Leopold dentro del pipeline de GoodMorning con autonomía, trazabilidad y límites seguros.

## Theme-aware UI implementation

- `shared/rendering.json` is the source of truth for semantic theme roles, Light/Dark neutral schemes, and the seven named accent palettes. Do not add component-specific hard-coded theme colors; resolve normal, selected, expanded, focused, pressed, and disabled treatments through shared palette tokens.
- Accent color and Light/Dark appearance are independent saved settings. The default accent is **Blue**, whose bright interactive tone preserves the existing cyan product appearance. Keep this default when no accent preference exists.
- The user changes accent in **Display → Appearance**. Keep all seven choices named and visibly selected; never rely on hue alone to communicate state.
- Theme-aware controls must retain readable text and outlines in both schemes. Check contrast and color accessibility, especially yellow in Light mode, and account for the actual display resolution, touch targets, and device navigation gestures.
- Store the accent through the existing settings mechanism and restore it at boot. Keep browser/simulator and firmware adapters aligned with the shared setting order and defaults.
- For every new or changed control: inspect the shared theme and existing reusable renderer first; define its semantic state colors in the shared contract; implement the same state behavior in the simulator/shared LVGL and device adapters; then review both schemes, all seven accents, selected/focus/press/disabled states, contrast, persistence/restart, and the real input path. Rebuild synchronized assets and the LVGL/WASM renderer when those inputs change, and report simulator, firmware, and physical-device evidence as separate gates.

## Home controls and shared status banner

- Follow `docs/parity/home-screen-button-sop.md` for Home glass-button appearance, assigned/reactive icons, press/cancel feedback, hit bounds, and destinations.
- Follow `docs/parity/shared-status-banner-sop.md` for the Settings-style shared banner. Its full 181 × 35 shell appears on every page except Home and Clock; do not clip the lower edge or reintroduce the obsolete 181 × 28 asset.
- Keep both contracts in the shared rendering and asset sources consumed by the simulator and device. The focused personal skills are `$leopold-home-controls` and `$leopold-status-banner`; the repository SOPs remain the project record.
- The Home-control contract also preserves existing assigned icons and brand colors, requires visible active/inactive behavior where shared state exists, and returns press feedback to rest on release or cancellation.
- The shared-banner contract uses the Settings pill as the gold standard at 181 × 35 px and requires full-height visibility on eligible pages. Keep Home's centered status pill and Clock's separate design.

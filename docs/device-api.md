# Leopold Device Cloud API v1

API HTTPS mínima para aprovisionar hasta 100 dispositivos Leopold, registrar su
presencia y telemetría, recibir eventos y entregar una interfaz Server-Driven UI
(SDUI). La URL prevista de desarrollo es
`https://api.dev.leopold.goodmorning.mx`; el dominio es configuración de cada
entorno y no forma parte del protocolo.

## Configuración del backend

Variables de ejecución:

```env
LEOPOLD_ADMIN_PROVISIONING_TOKEN=
LEOPOLD_DEVICE_OFFLINE_AFTER_SECONDS=90
LEOPOLD_UI_POLL_INTERVAL_SECONDS=5
LEOPOLD_HEARTBEAT_INTERVAL_SECONDS=30
```

El token administrativo debe ser un secreto aleatorio administrado fuera de
Git. Si está vacío o ausente, el aprovisionamiento responde `503` y permanece
deshabilitado. Los intervalos predeterminados indican al firmware cada cuánto
enviar heartbeat y consultar UI. `LEOPOLD_DEVICE_OFFLINE_AFTER_SECONDS` define
el umbral que un proceso futuro podrá usar para materializar el estado
`offline`; este MVP conserva `last_seen_at` como fuente de verdad y no añade un
worker ni Redis.

Todas las llamadas del dispositivo deben usar HTTPS en ambientes compartidos o
remotos. Coolify termina TLS y enruta hacia el puerto interno `8000`.

## Crear el primer dispositivo

1. Configura `LEOPOLD_ADMIN_PROVISIONING_TOKEN` en el entorno **backend**.
2. Ejecuta una sola vez (sustituye los valores locales):

   ```bash
   curl --fail-with-body \
     -X POST 'https://api.dev.leopold.goodmorning.mx/v1/admin/devices' \
     -H 'Authorization: Bearer <TOKEN_ADMINISTRATIVO>' \
     -H 'Content-Type: application/json' \
     --data '{"label":"Leopold prototipo 01"}'
   ```

3. Copia inmediatamente `device_id` y `device_secret` de la respuesta a la
   configuración local, no versionada, del firmware. El secreto se muestra una
   sola vez; la base de datos guarda únicamente su SHA-256 y ninguna API lo
   vuelve a exponer.
4. Borra el secreto del historial de terminal si el método usado para invocar
   la API pudiera haberlo guardado. No lo incluyas en logs, tickets o Git.

La URL base también debe copiarse a la configuración local del firmware. El
dispositivo nunca debe depender de una URL hardcodeada fuera de ese archivo
local configurable. Esta tarea no modifica `device/` ni instala credenciales en
el hardware.

## Autenticación

Cada llamada a `/v1/device/*` incluye:

```http
X-Leopold-Device-ID: LPD-A82F91
Authorization: Bearer <device_secret>
```

El servidor compara hashes en tiempo constante. Un ID inexistente y un secreto
incorrecto producen la misma respuesta `401`, sin indicar qué dato falló. Un
dispositivo autenticado con estado `disabled` recibe `403`.

## Conexión y presencia

```bash
curl --fail-with-body \
  -X POST "$LEOPOLD_API_URL/v1/device/connect" \
  -H "X-Leopold-Device-ID: $LEOPOLD_DEVICE_ID" \
  -H "Authorization: Bearer $LEOPOLD_DEVICE_SECRET" \
  -H 'Content-Type: application/json' \
  --data '{"firmware_version":"0.3.0","capabilities":{"display":"466x466","touch":true,"audio":false,"battery":true}}'
```

Respuesta `200`:

```json
{
  "device_id": "LPD-A82F91",
  "server_time": "2026-07-29T08:00:00Z",
  "heartbeat_interval_seconds": 30,
  "ui_poll_interval_seconds": 5,
  "ui_revision": 1,
  "claimed": false,
  "onboarding_url": "https://dev.leopold.goodmorning.mx/setup/<TOKEN>"
}
```

Actualiza firmware, capacidades, IP, `last_seen_at` UTC y estado `online`.

## Documento SDUI v1

```bash
curl --fail-with-body "$LEOPOLD_API_URL/v1/device/ui" \
  -H "X-Leopold-Device-ID: $LEOPOLD_DEVICE_ID" \
  -H "Authorization: Bearer $LEOPOLD_DEVICE_SECRET"
```

Contrato exacto inicial:

```json
{
  "schema_version": 1,
  "revision": 1,
  "poll_after_ms": 5000,
  "screen": {
    "id": "home",
    "background": "#000000",
    "components": [
      {"type":"text","id":"greeting","text":"Buenos días","x":233,"y":105,"align":"center","size":3,"color":"#FFFFFF"},
      {"type":"indicator","id":"cloud","label":"GoodMorning Cloud","state":"connected","x":233,"y":230},
      {"type":"button","id":"continue","label":"Continuar","x":133,"y":315,"width":200,"height":58}
    ]
  }
}
```

Reglas de compatibilidad:

- `schema_version` es `1`; el firmware debe rechazar de forma segura versiones
  mayores que no entienda.
- El lienzo es `466 × 466`; `x` e `y` están entre `0` y `465`.
- Colores con forma `#RRGGBB`.
- Tipos permitidos: `text`, `indicator`, `button`, `progress`, `slider` e
  `icon`. El firmware ignora tipos desconocidos para permitir extensiones
  compatibles.
- Todo componente contiene `type`, `id`, `x` e `y`.
- El cliente debe ignorar campos desconocidos para admitir extensiones
  compatibles.
- `revision` crece cuando cambia el documento y solo una revisión está activa.

La respuesta incluye `ETag: "1"`. En consultas posteriores:

```bash
curl -i "$LEOPOLD_API_URL/v1/device/ui" \
  -H "X-Leopold-Device-ID: $LEOPOLD_DEVICE_ID" \
  -H "Authorization: Bearer $LEOPOLD_DEVICE_SECRET" \
  -H 'If-None-Match: "1"'
```

Si sigue activa la revisión 1, devuelve `304 Not Modified` sin documento.

## Assets compartidos del dispositivo

El directorio raíz `shared/` es la única fuente de verdad para la interfaz del
simulador y del firmware. El manifiesto `shared/asset-manifest.json` enumera
los archivos permitidos y `shared/assets/` contiene sus bytes originales. El
script `scripts/sync_shared_assets.py` genera las copias de consumo, incluida
`backend/assets/`, que se empaqueta en la imagen Docker del API.

Cuando un asset no cabe de forma fiable en la memoria persistente del
dispositivo, el firmware lo descarga desde el backend autenticado:

```bash
curl --fail-with-body "$LEOPOLD_API_URL/v1/device/assets/icon_clock.bin" \
  -H "X-Leopold-Device-ID: $LEOPOLD_DEVICE_ID" \
  -H "Authorization: Bearer $LEOPOLD_DEVICE_SECRET" \
  -o icon_clock.bin
```

El backend solo sirve nombres presentes en `shared/asset-manifest.json`,
devuelve exactamente los bytes de `shared/assets/` y responde con
`Cache-Control: private, max-age=31536000, immutable`. El simulador usa las
copias sincronizadas del mismo manifiesto y assets; no se mantienen
definiciones de pantalla ni assets alternativos por plataforma.

## Publicar una nueva pantalla remotamente

El endpoint administrativo `PUT /v1/admin/devices/{device_id}/ui` acepta el
objeto `screen` directamente, o un documento SDUI v1 con `schema_version`,
`poll_after_ms` y `screen`, pero sin `revision`. El servidor bloquea la revisión
activa, desactiva la anterior y asigna la siguiente revisión en una sola
transacción. La respuesta contiene el documento completo y su nuevo `ETag`.

Ejemplo para reemplazar remotamente la pantalla de Leopold usando solo
`screen`:

```bash
curl --fail-with-body \
  -X PUT "$LEOPOLD_API_URL/v1/admin/devices/$LEOPOLD_DEVICE_ID/ui" \
  -H "Authorization: Bearer $LEOPOLD_ADMIN_PROVISIONING_TOKEN" \
  -H 'Content-Type: application/json' \
  --data '{
    "id":"remote-home",
    "background":"#001122",
    "components":[
      {
        "type":"text",
        "id":"remote-message",
        "text":"Pantalla actualizada",
        "x":233,
        "y":120,
        "align":"center",
        "size":3,
        "color":"#FFFFFF"
      }
    ]
  }'
```

Si la revisión activa era `1`, la respuesta tiene `revision: 2` y
`ETag: "2"`. El endpoint devuelve `401` para un token ausente o incorrecto,
`404` si no existe `device_id`, `409` si otra publicación mantiene bloqueada o
entra en conflicto con la revisión activa y `422` para un documento inválido.

## Eventos

```bash
curl --fail-with-body -X POST "$LEOPOLD_API_URL/v1/device/events" \
  -H "X-Leopold-Device-ID: $LEOPOLD_DEVICE_ID" \
  -H "Authorization: Bearer $LEOPOLD_DEVICE_SECRET" \
  -H 'Content-Type: application/json' \
  --data '{"event_id":"evt-001","revision":1,"type":"tap","component_id":"continue","payload":{},"occurred_at":"2026-07-29T08:01:00Z"}'
```

Devuelve `202`. Repetir `event_id` para el mismo dispositivo es idempotente y
no crea otra fila. El MVP acepta solo `tap`, limita identificadores y payloads,
y no ejecuta integraciones externas.

## Heartbeat

```bash
curl --fail-with-body -X POST "$LEOPOLD_API_URL/v1/device/heartbeat" \
  -H "X-Leopold-Device-ID: $LEOPOLD_DEVICE_ID" \
  -H "Authorization: Bearer $LEOPOLD_DEVICE_SECRET" \
  -H 'Content-Type: application/json' \
  --data '{"firmware_version":"0.3.0","wifi_rssi":-54,"battery_percent":82,"uptime_seconds":150}'
```

## Onboarding web por QR

`POST /v1/device/connect` devuelve `claimed` y, para dispositivos sin
propietario, un `onboarding_url` HTTPS de un solo uso. El token expira en 15
minutos y se almacena únicamente como SHA-256.

El firmware muestra ese enlace como QR y consulta:

```http
POST /v1/device/onboarding/status
```

Cuando responde `{"claimed":true}`, Leopold deja el QR y continúa con la UI
remota.

La web usa los siguientes endpoints:

- `POST /v1/auth/register`
- `POST /v1/auth/login`
- `POST /v1/auth/logout`
- `GET /v1/auth/me`
- `GET /v1/onboarding/{token}`
- `POST /v1/onboarding/{token}/claim`
- `GET /v1/me/devices`

Las contraseñas usan `scrypt` con salt aleatorio. Las sesiones son tokens
aleatorios almacenados como hash y entregados en cookie `HttpOnly`, `Secure` en
entornos remotos y `SameSite=Lax`. El token administrativo de aprovisionamiento
no forma parte de este flujo y nunca se entrega al navegador.

Respuesta `200`:

```json
{"ok":true,"server_time":"2026-07-29T08:02:00Z","ui_revision":1}
```

Actualiza telemetría, IP, presencia y estado. Los cuerpos `/v1` con
`Content-Length` mayor de 16 KiB reciben `413`; valores fuera de contrato
reciben `422`.

## Errores esperados

| Estado | Significado |
| --- | --- |
| `401` | Token administrativo o credenciales del dispositivo ausentes/incorrectos. |
| `403` | Dispositivo autenticado pero deshabilitado. |
| `404` | Ruta inexistente. |
| `413` | Cuerpo declarado mayor de 16 KiB. |
| `422` | JSON, tipos, límites o contrato inválidos. |
| `503` | Aprovisionamiento deshabilitado o UI activa no disponible. |

Device Cloud puede componer experiencias conectadas como Spotify y YouTube en
un mismo documento. YouTube usa OAuth de sólo lectura para mostrar actividad de
suscripciones; YouTube Data API no ofrece controles de reproducción remota
equivalentes a Spotify Connect. No existe aún audio, LLM, WebSocket, OTA, app
móvil, Redis ni panel administrativo visual.

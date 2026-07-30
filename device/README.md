# Leopold device firmware

Primer MVP conectado para **Waveshare ESP32-S3-Touch-AMOLED-1.43-B**. El
firmware configura Wi‑Fi desde un teléfono, autentica el dispositivo por HTTPS,
renderiza Server-Driven UI (SDUI) y conserva la última interfaz válida cuando
la nube no está disponible.

## Hardware objetivo

- ESP32-S3R8, 16 MB flash y 8 MB PSRAM.
- AMOLED circular 466 × 466 por QSPI.
- Touch I2C `0x38` en SDA 47 / SCL 48.
- AMOLED EN 42 y BOOT GPIO 0.
- Batería por ADC GPIO 4.
- Sin micrófono, bocina ni códec de audio integrados.

El pinout está centralizado en `include/board_config.h`. La unidad validada se
anuncia comercialmente como SH8601/CO5300 y necesita la secuencia
`Arduino_CO5300`. Algunas revisiones pueden requerir `Arduino_SH8601`; confirma
la revisión física antes de cambiar el controlador en `src/display_manager.cpp`.

## Instalar PlatformIO

Instala PlatformIO Core o la extensión PlatformIO IDE para VS Code. Para
verificar la instalación:

```bash
pio --version
```

En esta estación de desarrollo también puede usarse:

```bash
/home/goodmorning/.platformio/penv/bin/pio --version
```

Las versiones de ArduinoJson, WiFiManager y Arduino_GFX están fijadas en
`platformio.ini` para obtener builds reproducibles.

## Configuración segura del dispositivo

Copia el ejemplo:

```bash
cd device
cp include/leopold_config.example.h include/leopold_config.h
```

Edita únicamente `include/leopold_config.h`:

```cpp
#define LEOPOLD_API_BASE_URL "https://api.dev.leopold.goodmorning.mx"
#define LEOPOLD_DEVICE_ID "LPD-A82F91"
#define LEOPOLD_DEVICE_SECRET "valor-entregado-por-el-backend"
```

El `device_id` y `device_secret` deben obtenerse mediante el flujo autorizado de
aprovisionamiento de GoodMorning Device Cloud. El firmware no crea identidades
ni inventa secretos. Si no tienes acceso a ese flujo, solicita al responsable
del backend que aprovisione el dispositivo.

`include/leopold_config.h` está ignorado por Git. El build falla si falta o si
mantiene `CHANGE-ME`. Nunca copies credenciales reales al archivo de ejemplo.

El ejemplo incluye el certificado público ISRG Root X1 para validar TLS. No se
usa `setInsecure()`. Si el API cambia de cadena de confianza, actualiza el
certificado raíz público después de verificar la nueva cadena.

## Compilar y cargar

```bash
cd device
pio run
pio run --target upload
pio device monitor --baud 115200
```

Para indicar el puerto manualmente:

```bash
pio run --target upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200
```

Para un reinicio completo durante testing, borra toda la flash antes de volver
a cargar. Esto elimina Wi‑Fi, NVS y la caché LittleFS:

```bash
pio run --target erase --upload-port /dev/ttyACM0
pio run --target upload --upload-port /dev/ttyACM0
```

Para repetir también el onboarding de cuenta, elimina la vinculación exacta del
dispositivo en el backend de `dev`. Nunca ejecutes esa operación en producción.

Si Linux niega acceso al puerto, agrega tu usuario al grupo `dialout` y vuelve a
iniciar sesión, o concede acceso temporal al dispositivo:

```bash
sudo setfacl -m u:"$USER":rw /dev/ttyACM0
```

Para entrar manualmente al bootloader: mantén `BOOT`, pulsa `RESET`, suelta
`RESET` y después suelta `BOOT`.

## Configurar Wi‑Fi desde el teléfono

1. Sin credenciales guardadas, Leopold crea inmediatamente `Leopold-XXXX` y
   muestra un QR de Wi‑Fi. Si hay credenciales, intenta usarlas durante
   aproximadamente 20 segundos antes de abrir el portal.
2. Escanea el QR con la cámara del teléfono para unirte a esa red.
3. El portal cautivo se abre automáticamente. Si no aparece, abre
   `http://192.168.4.1`.
4. Selecciona una red, escribe su contraseña y guarda.
5. Leopold detiene el portal y muestra `WIFI CONECTADO`.

La contraseña Wi‑Fi no se muestra en pantalla ni se imprime por Serial.

## Vincular Leopold a una cuenta

Después de conectarse a Wi‑Fi, un Leopold sin propietario solicita al backend
un enlace HTTPS de un solo uso y lo muestra como QR. El enlace expira después de
15 minutos. Desde el teléfono:

1. Escanea el QR `ACTIVA LEOPOLD`.
2. Crea una cuenta o inicia sesión.
3. Pulsa `Vincular Leopold`.
4. El firmware consulta el estado cada cinco segundos y continúa
   automáticamente cuando el backend confirma el vínculo.

La contraseña de la cuenta nunca llega al dispositivo. El firmware conserva
únicamente su identidad técnica; el backend almacena hashes de contraseñas,
sesiones y tokens de claim. Los QR usados o vencidos no pueden reutilizarse.

Para borrar solo la configuración Wi‑Fi, mantén `BOOT` durante cinco segundos.
Leopold conserva su identidad, borra las redes guardadas y reinicia el portal.
Una pulsación corta de `BOOT` sigue siendo una interacción de respaldo: activa
el primer botón SDUI o cambia la expresión local si no hay una UI remota.

## Estados visibles

- `BOOTING`: inicialización local.
- `WIFI_CONNECTING`: conexión a credenciales guardadas.
- `WIFI_PORTAL`: portal `Leopold-XXXX`.
- `CLOUD_CONNECTING`: autenticación HTTPS en curso.
- `ONLINE`: nube autenticada y polling SDUI activo.
- `OFFLINE`: conserva la última UI válida y reintenta con backoff.
- `ERROR`: configuración inválida, autenticación rechazada o fallo no
  recuperable.

La barra superior reserva indicadores discretos de Wi‑Fi, nube, batería y hora.
La hora muestra `--:--` hasta que NTP sincroniza.

## Server-Driven UI

El renderer v1 acepta como máximo 12 componentes y 12 KiB de JSON:

- `text`
- `indicator`
- `button`
- `progress`

Los colores `#RRGGBB` se convierten localmente a RGB565. Los componentes
desconocidos se ignoran; documentos inválidos o demasiado grandes se rechazan
antes de dibujar. La última UI válida se guarda en LittleFS y permanece visible
durante pérdidas de Wi‑Fi o nube. `If-None-Match` evita descargar revisiones sin
cambios.

Los botones generan eventos con identificadores únicos en una cola limitada a
cuatro elementos. Cada evento tiene hasta tres intentos y la cola nunca crece
sin límite.

## Diagnóstico por Serial

El monitor muestra:

- versión de firmware;
- transiciones de estado;
- rechazo de documentos SDUI inválidos;
- fallos de caché;
- descarte de eventos cuando la cola está llena;
- borrado de configuración Wi‑Fi.

No imprime contraseñas, `Authorization` ni el secreto del dispositivo. Las
peticiones HTTPS se ejecutan en una tarea FreeRTOS separada con timeout de cinco
segundos para no congelar display o touch.

## Limitaciones del MVP

- No incluye audio, Spotify, LLM, OTA, BLE ni aplicación móvil.
- El porcentaje de batería es una estimación lineal calibrable entre 3.3 V y
  4.2 V con divisor nominal 2:1. No debe interpretarse como medición de precisión.
- Los eventos offline se conservan solo en RAM y se descartan al reiniciar.
- La hora usa la zona fija de Ciudad de México (`CST6`).
- La validación física de portal, touch, ADC y endpoints requiere hardware
  aprovisionado y un backend accesible.

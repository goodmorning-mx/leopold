# Leopold device firmware

Firmware mínimo para el dispositivo físico Leopold basado en **Waveshare ESP32-S3-Touch-AMOLED-1.43-B**.

## Qué hace esta versión

- Inicializa la pantalla AMOLED circular de 466 × 466.
- Muestra la identidad visual básica de Leopold.
- Incluye tres estados locales: `LISTO`, `TE ESCUCHO` y `PENSANDO`.
- Cambia de estado tocando la pantalla o presionando `BOOT`.
- Ejecuta una animación sencilla de parpadeo.
- Expone mensajes de diagnóstico por USB a 115200 baudios.

Este primer firmware no contiene credenciales, autenticación, Wi-Fi, audio ni lógica de negocio. La conexión con GoodMorning Cloud y el protocolo de Server-Driven UI se incorporarán después sobre esta base.

## Hardware objetivo

- ESP32-S3R8
- Flash: 16 MB
- PSRAM: 8 MB
- AMOLED: 1.43 pulgadas, 466 × 466, controlador SH8601
- Touch I2C: dirección `0x38`

El pinout está centralizado en `include/board_config.h`.

## Compilar

Requiere [PlatformIO](https://platformio.org/).

```bash
cd device
pio run
```

## Cargar por USB-C

```bash
cd device
pio run --target upload
pio device monitor
```

Si PlatformIO no detecta el puerto automáticamente, conecta el dispositivo por USB-C, mantén presionado `BOOT`, pulsa `RESET`, suelta `RESET` y finalmente suelta `BOOT`. Después repite la carga indicando el puerto si fuera necesario.

> Algunas revisiones del módulo usan el controlador CO5300. Esta versión inicial apunta al SH8601 del hardware validado; si la pantalla permanece negra, confirma la revisión antes de cambiar el driver.

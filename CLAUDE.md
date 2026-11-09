# APRS LoRa+ Display — contexto del proyecto

Propietario: Manuel (EA7JRS). Idioma de trabajo: español, respuestas breves y directas.
Archivos nuevos con la cabecera `Copyright (C) 2026 Manuel - EA7JRS` y licencia GPL-3.0-or-later.
Repositorio: `EA7JRS/APRS-LoRa-Plus-Display`. Hermanos: tracker `EA7JRS/APRS-LoRa-Plus`, app `…-Android`, rotor `…-Rotor`.

## Qué es
Pantalla grande y táctil para el tracker: **Waveshare ESP32-S3-Touch-LCD-7B** (1024×600, GT911), enlace **BLE** con el servicio de control del tracker (`docs/BLE_CONTROL.md` del firmware, mismo PIN que la app). Control completo, no solo ver (pedido de Manuel, 2026-11-08). Misma versión que el resto: **5.3.5** (`DISPLAY_VERSION` en `platformio.ini`).

## Estructura
- `src/protocol.*`: C++ puro (modelo y órdenes del tracker), probado en el PC: `test/test_protocol.cpp` (40 comprobaciones; `g++ -std=c++17 -I src -I <ArduinoJson>/src test/test_protocol.cpp src/protocol.cpp`).
- `src/board_lcd7b.*`: panel RGB, táctil, expansor. `src/tracker_link.*`: cliente NimBLE con PIN. `src/ui.*`: LVGL 9.6 (desde 2026-11-09; antes 8.4). `src/main.cpp`. `src/ui_assets.*`: icono de la app y pantalla de arranque (bienvenida de la app), generados por `tools/make_ui_assets.py` desde `tools/assets/`. `src/lv_defs.h`: ajustes de LVGL que no van en `platformio.ini` (`LV_FONT_DEFAULT`, formato RGB565, malloc de la libreria C); no hay `lv_conf.h`.
- Plataforma `espressif32 @ 8.0.0` (registro de PlatformIO; las descargas de GitHub Releases de pioarduino daban 504). `Serial` = UART0 (CH343), no USB nativo (compartido con CAN).
- Instalador web: `tools/make_web_assets.py` + `release.yml` (etiqueta `v5.3.5` → `display-waveshare_lcd7b.zip` y `web-site-display.zip`); el job `pages` del tracker lo descarga con la misma etiqueta (secreto `FLASHER_TOKEN`). Por USB contesta a `APRS-INFO?` con `device=display`.

## Estado (2026-11-08)
- Compila en la CI (1,45 MB, 22 % de la partición) y pasa las pruebas del protocolo. 2026-11-09: en la placa la pantalla salía negra; arreglado el expansor (EXIOn = bit n, PWM al revés) según el código de Waveshare (`waveshareteam/ESP32-S3-Touch-LCD-7B`). Pendiente de confirmar en la placa. La interfaz se puede probar en el PC con LVGL 9.6 real y placa/enlace simulados (capturas RGB565 de 1024×600).

## Cómo trabajar
- No publicar releases ni tocar otros repositorios sin que lo pida Manuel (las etiquetas y releases las crea él).

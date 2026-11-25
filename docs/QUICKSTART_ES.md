# APRS LoRa+ Display — guía rápida

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later. Versión **5.5.0**. Manual completo: [MANUAL_ES.md](MANUAL_ES.md).

## 1. Elige el firmware

| Tu placa Waveshare | Firmware |
|---|---|
| ESP32-S3-Touch-LCD-**7B** | `waveshare_lcd7b` (pestañas) o `waveshare_lcd7b_v2` (como la app) o `waveshare_lcd7b_hamclock` (reloj) |
| ESP32-S3-Touch-LCD-**7** (sin B) | `waveshare_lcd7` (pestañas) o `waveshare_lcd7_v2` (como la app) |
| ESP32-S3-Touch-LCD-3.5 / 3.5-C | `waveshare_lcd35` |
| ESP32-S3-Touch-LCD-2.8B | `waveshare_lcd28b` |
| ESP32-S3-Touch-LCD-1.69 | `waveshare_lcd169` |

La 7 y la 7B no se cruzan: con el firmware de la otra, pantalla negra.

## 2. Instala

1. Chrome o Edge: **https://ea7jrs.github.io/APRS-LoRa-Plus/install/#display**.
2. Placa por USB (en la 7 y la 7B, el USB-C de la UART) → *Detectar placa* → firmware → *Instalar*.

## 3. Únela al tracker

1. Tracker: WebUI > Bluetooth encendido; apunta el *Control PIN* (de serie `123456`).
2. Pantalla: *Settings* (en la v2, *Settings > TRACKER LINK*) → el PIN → guardar. Se empareja y vuelve a unirse sola.
3. Desconecta la app del móvil: el tracker solo admite un equipo de control a la vez.

## 4. WiFi (opcional)

- Pestañas: pestaña **Network** → red, clave y servidor (NMEA 10110, gpsd 2947, Signal K 8375 o vacío para el móvil).
- v2: *Settings > WIFI*. Con ella: DX Cluster, tiempo de OpenWeatherMap (clave gratuita), mapa, avisos y firmware del tracker.

## 5. Uso diario

| Para | Pestañas | v2 |
|---|---|---|
| Ver estaciones | Heard | S.LIST |
| Mensajes | Messages | MSG |
| Baliza ahora | Status | BCN |
| Cambiar modo | Status | [F] > MODE |
| Rotor | SAT / Rotor | ROTOR |
| Avisos oficiales | Sensors | [F] > WARNINGS |
| Mapa | — | MAP |

Esfera del rotor: azul = posición, ámbar = objetivo, verde = rumbo del tracker en marcha, rojo = topes.

# APRS LoRa+ Display

[![Donate · PayPal.me](https://img.shields.io/badge/Donate-PayPal.me%2FEA7JRS-0070E0?logo=paypal&logoColor=white)](https://paypal.me/EA7JRS)

Pantalla táctil para el tracker **APRS LoRa+** ([EA7JRS/APRS-LoRa-Plus](https://github.com/EA7JRS/APRS-LoRa-Plus)). Se conecta al tracker por **Bluetooth LE**, con el mismo servicio de control y el mismo PIN que la app de Android (`docs/BLE_CONTROL.md` del firmware).

| Placa Waveshare | Pantalla | Entorno |
|---|---|---|
| ESP32-S3-Touch-LCD-7B | 7", 1024×600 | `waveshare_lcd7b` |
| ESP32-S3-Touch-LCD-3.5 y 3.5-C | 3,5", 480×320 (horizontal) | `waveshare_lcd35` |
| ESP32-S3-Touch-LCD-2.8B | 2,8", 480×640 (vertical) | `waveshare_lcd28b` |
| ESP32-S3-Touch-LCD-1.69 | 1,69", 240×280 | `waveshare_lcd169` |

La interfaz tiene el formato de la pantalla del rotor (tarjetas, listas clave / valor, brújula, DejaVu Sans) con la paleta de la app Android o la del rotor (Settings > Theme), es la misma en todas y se adapta al tamaño (en las pequeñas las pestañas son iconos y las tablas tienen menos columnas). Pines y lo que falta por verificar de cada placa: `docs/HARDWARE.md`.

No es un tracker: no lleva radio LoRa ni GPS. Solo muestra los datos del tracker y lo controla.

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later.

## Pestañas

| Pestaña | Qué hace |
|---|---|
| Status | Posición GNSS, modo, perfil, batería. Baliza ahora, digipeater, cambio de modo (reinicia el tracker) |
| Heard | Estaciones oídas por el tracker: distancia desde el GNSS del tracker, RSSI, SNR, antigüedad, comentario |
| Messages | Registro de mensajes APRS y envío con teclado táctil (`msg`) |
| Sensors | Batería, temperatura, humedad, presión, QNH, altitud barométrica, rumbo |
| SAT / Rotor | Satélite en curso (azimut, elevación, distancia) y objetivo del rotor |
| Mirror | Espejo de la pantalla del tracker (`screen`) |
| DAPNET | Avisos recibidos por el tracker en modo DAPNET |
| Settings | PIN del tracker, dirección del tracker, olvidar emparejamiento, luz de fondo |

## Enlace con el tracker

1. En el tracker: BLE activado (WebUI → Bluetooth) y, mejor, un *Control PIN* propio (por defecto `123456`).
2. La pantalla busca el primer tracker que anuncie el servicio KISS, TNC2 o de control y se empareja con el PIN guardado (Settings). Si el PIN cambia en el tracker: **Forget pairing** y volver a conectar.
3. Si hay varios trackers, escribir su dirección BLE en Settings.

## Compilar

```
pio run -e waveshare_lcd7b      # o waveshare_lcd35, waveshare_lcd28b, waveshare_lcd169
```

Usa pioarduino (Arduino-ESP32 3.x), LVGL 9.6, NimBLE-Arduino 2.5 y ArduinoJson 7.4. La CI compila en cada subida y ejecuta las pruebas del protocolo en el PC (`test/test_protocol.cpp`).

## Instalar

Desde el instalador web del proyecto (Chrome o Edge, placa por USB): https://ea7jrs.github.io/APRS-LoRa-Plus/#display

## Estado

**Sin probar en las placas.** Ver `docs/HARDWARE.md` para lo que está sin verificar de cada una antes del primer arranque.

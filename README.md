# APRS LoRa+ Display

Pantalla grande y táctil para el tracker **APRS LoRa+** ([EA7JRS/APRS-LoRa-Plus](https://github.com/EA7JRS/APRS-LoRa-Plus)). Funciona sobre la **Waveshare ESP32-S3-Touch-LCD-7B** (1024×600) y se conecta al tracker por **Bluetooth LE**, con el mismo servicio de control y el mismo PIN que la app de Android (`docs/BLE_CONTROL.md` del firmware).

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
pio run -e waveshare_lcd7b
```

Usa pioarduino (Arduino-ESP32 3.x), LVGL 8.4, NimBLE-Arduino 2.x y ArduinoJson 7. La CI compila en cada subida y ejecuta las pruebas del protocolo en el PC (`test/test_protocol.cpp`).

## Estado

**Sin probar en la placa.** Ver `docs/HARDWARE_LCD7B.md` para lo que está sin verificar (expansor de E/S, tiempos del panel) antes del primer arranque.

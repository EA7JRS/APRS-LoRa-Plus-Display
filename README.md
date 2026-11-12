# APRS LoRa+ Display

[![Donate · PayPal.me](https://img.shields.io/badge/Donate-paypal.me%2Fea7jrs-0070E0?logo=paypal&logoColor=white)](https://paypal.me/ea7jrs)

Pantalla táctil para el tracker **APRS LoRa+** ([EA7JRS/APRS-LoRa-Plus](https://github.com/EA7JRS/APRS-LoRa-Plus)). Se conecta al tracker por **Bluetooth LE**, con el mismo servicio de control y el mismo PIN que la app de Android (`docs/BLE_CONTROL.md` del firmware).

| Placa Waveshare | Pantalla | Entorno |
|---|---|---|
| ESP32-S3-Touch-LCD-7B | 7", 1024×600 | `waveshare_lcd7b` |
| ESP32-S3-Touch-LCD-3.5 y 3.5-C | 3,5", 480×320 (horizontal) | `waveshare_lcd35` |
| ESP32-S3-Touch-LCD-2.8B | 2,8", 480×640 (vertical) | `waveshare_lcd28b` |
| ESP32-S3-Touch-LCD-1.69 | 1,69", 240×280 | `waveshare_lcd169` |

La interfaz tiene el formato de la pantalla del rotor (tarjetas, listas clave / valor, brújula, DejaVu Sans) con la paleta de la app Android o la del rotor (Settings > Theme), es la misma en todas y se adapta al tamaño (en las pequeñas las pestañas son iconos y las tablas tienen menos columnas). Pines y lo que falta por verificar de cada placa: `docs/HARDWARE.md`.

No es un tracker: no lleva radio LoRa ni GPS. Muestra los datos del tracker y lo controla, y por **WiFi** recoge los datos del barco de la red local (gpsd, Signal K, NMEA 0183 por TCP o UDP, el NMEA del móvil, el instrumento de viento), con la misma conectividad que el plotter.

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
| Network | Datos del barco de la red local (posición, velocidad, rumbo, sonda, viento, AIS con CPA) con su origen, estado de la WiFi y del servidor, y su configuración |
| Settings | PIN del tracker, dirección del tracker, olvidar emparejamiento, luz de fondo |

## Enlace con el tracker

1. En el tracker: BLE activado (WebUI → Bluetooth) y, mejor, un *Control PIN* propio (por defecto `123456`).
2. La pantalla busca el primer tracker que anuncie el servicio KISS, TNC2 o de control y se empareja con el PIN guardado (Settings). Si el PIN cambia en el tracker: **Forget pairing** y volver a conectar.
3. Si hay varios trackers, escribir su dirección BLE en Settings.

## Red local (pestaña Network)

La misma conectividad que el plotter náutico ([EA7JRS/APRS-LoRa-Plus-Plotter](https://github.com/EA7JRS/APRS-LoRa-Plus-Plotter)): WiFi cliente, **un servidor TCP** y escucha **UDP**, en una tarea aparte (una red caída no para la pantalla).

| Servidor | Puerto | Qué llega |
|---|---|---|
| NMEA 0183 | 10110 | Sentencias NMEA y AIS (`!AIVDM`) de cualquier equipo de la red |
| gpsd | 2947 | NMEA del receptor de gpsd (`?WATCH`) |
| Signal K | 8375 | Deltas del barco propio (suscripción a `vessels.self`); avisos `notifications.*` en estado *alarm* / *emergency* |
| Dirección vacía | 10110 | El móvil que da la WiFi (punto de acceso) con la app APRS LoRa+ y su servidor NMEA encendido |

Por **UDP** (por ejemplo, 10110) entra el NMEA que se difunde en la red, como el del instrumento de viento de 4″. Con una clave de **OpenWeatherMap**, viento, presión y temperatura de internet en la posición del barco (en ámbar: estimado).

Cada dato se toma de la mejor fuente que esté al día: red (LAN), tracker por BLE (TRK, solo la posición, el rumbo y la velocidad de su GNSS) e internet (WEB). A los 10 s sin la fuente mejor entra la siguiente. Los blancos AIS de la red salen con distancia, demora y CPA (riesgo con CPA ≤ 0,5 mn y TCPA ≤ 20 min, en rojo).

**Hora**: el reloj de la pantalla se pone con la del GNSS del tracker (evento `gnss`) o la del GPS que llegue por la red (RMC / ZDA) y, sin GPS, con NTP por la WiFi: `ntp.roa.es`, la hora oficial de España (Real Instituto y Observatorio de la Armada), con `hora.roa.es` y `pool.ntp.org` de reserva. Mientras la del GPS esté al día (6 h), NTP no la sustituye. Settings > Link > *Clock* enseña la hora UTC y de dónde sale.

La configuración se guarda en la pantalla (WiFi, servidor, puertos, clave). El código común está en `lib/plotter_core` y `lib/aprs_links`, copiado del plotter con `tools/sync_links.sh` (se cambia allí, donde tiene sus pruebas, y se vuelve a copiar). El Bluetooth NMEA del plotter (pasarela XIAO, instrumento de viento) no está activado aquí todavía: la pantalla ya usa el Bluetooth para el tracker.

## Compilar

```
pio run -e waveshare_lcd7b      # o waveshare_lcd35, waveshare_lcd28b, waveshare_lcd169
```

Usa pioarduino (Arduino-ESP32 3.x), LVGL 9.6, NimBLE-Arduino 2.5 y ArduinoJson 7.4. La CI compila en cada subida y ejecuta las pruebas del protocolo en el PC (`test/test_protocol.cpp`).

## Instalar

Desde el instalador web del proyecto (Chrome o Edge, placa por USB): https://ea7jrs.github.io/APRS-LoRa-Plus/#display

## Estado

**Sin probar en las placas.** Ver `docs/HARDWARE.md` para lo que está sin verificar de cada una antes del primer arranque.

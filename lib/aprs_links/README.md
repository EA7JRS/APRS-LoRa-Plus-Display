# aprs_links — conectividad común de APRS LoRa+

Librería que comparten el plotter, APRS LoRa+ Display y APRS LoRa+ Rotor. **El origen es este repositorio**: aquí se
cambia y se prueba, y los otros llevan una copia en `lib/` (`tools/sync_links.sh <checkout del plotter>` en cada uno, con el
commit en `lib/ORIGEN.txt`).

| Módulo | Qué hace |
|---|---|
| `net` | WiFi cliente con reintento, un servidor TCP (NMEA 0183, gpsd con `?WATCH` o Signal K por el puerto 8375; dirección vacía = el equipo que da la WiFi, p. ej. el móvil con la app), escucha UDP y consulta del tiempo de OpenWeatherMap. Tarea aparte en el núcleo 0; los bytes llegan al bucle principal por `StreamBuffer`, así una red caída no para la pantalla |
| `ble` | Bluetooth central: el servicio de datos NMEA de `docs/BLE_NMEA.md` (pasarela XIAO, instrumento de viento), el UART de Nordic y el tracker APRS LoRa+ (servicio de control con PIN). Recuerda el equipo; escribe hacia él por `…0002` (`ble::write`) |
| `sk_link` | Lector JSON de Signal K (saludo, deltas del barco propio, avisos) con ArduinoJson; los datos van a la tabla con `skApply` del núcleo |

Notas para quien la use:
- `net::Config.ownWifi = false`: la WiFi la lleva el programa (el rotor tiene su propio punto de acceso de rescate) y `net`
  solo la usa cuando está en marcha, como cliente o como punto de acceso.
- `aprs_net.h` es otro nombre de `net.h` para los proyectos que ya tienen un `net.h` propio en `src/`.
- `ble` se queda sin hacer nada fuera del ESP32, con `-DAPRS_LINKS_NO_BLE` o si NimBLE va sin el papel de central
  (`CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED`, el rotor).

No dependen de los ajustes ni de la pantalla de quien los use: se configuran con sus funciones `set` / `apply` y se leen
con `read` / `status`. Lo que se hace con los datos (prioridad de fuentes, qué se manda al instrumento) va en el
proyecto, en el plotter en `src/links.cpp`.

Copyright (C) 2026 Manuel - EA7JRS · GPL-3.0-or-later

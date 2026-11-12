# aprs_links — conectividad común de APRS LoRa+

Librería que comparten el plotter y, más adelante, APRS LoRa+ Display (y lo que haga falta del resto del grupo). Está
aquí mientras no exista el repositorio común; entonces se muda allí con `plotter_core` y los proyectos la toman por
`lib_deps`.

| Módulo | Qué hace |
|---|---|
| `net` | WiFi cliente con reintento, un servidor TCP (NMEA 0183, gpsd con `?WATCH` o Signal K por el puerto 8375; dirección vacía = el equipo que da la WiFi, p. ej. el móvil con la app), escucha UDP y consulta del tiempo de OpenWeatherMap. Tarea aparte en el núcleo 0; los bytes llegan al bucle principal por `StreamBuffer`, así una red caída no para la pantalla |
| `ble` | Bluetooth central: el servicio de datos NMEA de `docs/BLE_NMEA.md` (pasarela XIAO, instrumento de viento), el UART de Nordic y el tracker APRS LoRa+ (servicio de control con PIN). Recuerda el equipo; escribe hacia él por `…0002` (`ble::write`) |
| `sk_link` | Lector JSON de Signal K (saludo, deltas del barco propio, avisos) con ArduinoJson; los datos van a la tabla con `skApply` del núcleo |

No dependen de los ajustes ni de la pantalla de quien los use: se configuran con sus funciones `set` / `apply` y se leen
con `read` / `status`. Lo que se hace con los datos (prioridad de fuentes, qué se manda al instrumento) va en el
proyecto, en el plotter en `src/links.cpp`.

Copyright (C) 2026 Manuel - EA7JRS · GPL-3.0-or-later

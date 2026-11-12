# Changelog

## Sin publicar
- Registro de arranque con la línea común del ecosistema: «[INFO][Main] APRS LoRa+ (c) 2026 Manuel - EA7JRS».

## 5.4.4 — 2026-11-12
- Solo el número de versión (misma versión en todo el ecosistema; las novedades de la 5.4.4 son del tracker y la app).

## 5.4.3 — 2026-11-12
_La 5.4.2 no llegó a publicarse: todo lo preparado para ella sale en la 5.4.3._

- **Reloj con los datos de OpenHamClock** en la 7B: entorno nuevo `waveshare_lcd7b_hamclock` (`src/hamclock`, `docs/HAMCLOCK.md`). Hora UTC y local, meteorología espacial, condiciones por banda, mapa con la noche y los spots, DX cluster (al tocar un spot pasa a ser el DX), POTA / SOTA, pases de satélites y noticias DX, por WiFi desde la API de un servidor OpenHamClock (openhamclock.com o uno propio). Ajustes en pantalla. **CAT con Hamlib**: cliente de `rigctld` por WiFi (frecuencia y modo de la radio en pantalla; TUNE la lleva al spot o a la activación elegidos), probado contra `rigctld` 4.5.5 con la radio de pega. Modelo probado en el PC (89 comprobaciones) e interfaz dibujada con LVGL 9.6 real (`tools/hamclock_sim`). En el instalador web como placa `waveshare_lcd7b_hamclock` del Display. Sin probar todavía en una 7B. Los demás entornos no cambian.
- **Fórmulas comunes del ecosistema** (de `lib/plotter_core` del plotter): Beaufort junto al viento (Network y Sensors), punto de rocío y sensación térmica junto a la temperatura, sol y luna del día en la posición (Network), y rumbo verdadero = magnético + declinación del WMM2025 si no llega uno verdadero.
- **Hora**: el reloj se pone con el GNSS del tracker o el GPS de la red (RMC / ZDA) y, sin GPS, con NTP: `ntp.roa.es` (hora oficial de España, ROA), de reserva `hora.roa.es` y `pool.ntp.org` (`tsync` y `timesync` de las librerías del plotter). Settings > Link > *Clock*.
- **Pestaña Network: red local con la conectividad del plotter** (`lib/plotter_core` y `lib/aprs_links`, copiadas de APRS-LoRa-Plus-Plotter con `tools/sync_links.sh`). WiFi cliente, un servidor TCP (NMEA 0183, gpsd o Signal K; dirección vacía = el móvil que da la WiFi, puerto 10110), escucha UDP y tiempo de OpenWeatherMap. Datos del barco con su origen (LAN, TRK, WEB) y prioridad de fuentes; la posición del GNSS del tracker entra también, por debajo de la red. Blancos AIS de la red con distancia, demora y CPA (riesgo en rojo); avisos de Signal K. Configuración guardada en la pantalla (`display/wifiSsid`, `tcpKind`...). En todas las placas. Sin probar en las placas ni con una red real.

## 5.4.1 — 2026-11-10
- Pestaña Sensors: pronóstico y tendencia de 3 h del tiempo de internet del tracker.
- **Reinicio en bucle arreglado** (7B, 5.4.0 y 5.4.1: «Stack canary watchpoint triggered (loopTask)»). LVGL 9 dibuja cada nivel de objetos con dos funciones que se llaman entre sí y con las pestañas anidadas no cabía en los 8 KB de pila de `loop()`; ahora 32 KB. Cada minuto se escribe por el puerto serie la pila que queda libre (`[main] pila libre`).
- La release adjunta `display-elf.zip` (el ELF de cada placa) para descodificar las trazas de un fallo.

## 5.4.0 — 2026-11-09
- **Dos paletas** a elegir en Settings > Theme (se guarda y la pantalla se reinicia): **App**, la de la app Android (`Theme.kt`: fondo #131A22, paneles #18222D con borde #263443, cian #28B5E1, verde #3CE68A para lo encendido, ámbar #FFB347 para lo elegido; cabecera oscura y títulos de tarjeta en mayúsculas espaciadas), que es la de serie, y **Rotor**, la de la pantalla del rotor (cabecera roja).
- **Aspecto del rotor** (APRS-LoRa-Plus-Rotor): su paleta oscura (fondo #10151c, tarjetas #19212b, azul #4aa3ff), cabecera roja con «APRS LoRa+» en negrita y debajo «Display 5.3.5», nombre del tracker y «BLE ●» a la derecha, listas clave / valor en tarjetas (Status, Sensors, Settings), brújula con N en rojo y AZ / EL grandes en SAT / Rotor, brillo con − / + y letra DejaVu Sans. Las fuentes (`src/fonts`, `tools/make_fonts.sh`) llevan Latin-1 (acentos, ñ, °); los iconos salen de la Montserrat.
- **Tres placas más**: Waveshare ESP32-S3-Touch-LCD-3.5 / 3.5-C (ST7796, 480×320 en horizontal, FT6336), ESP32-S3-Touch-LCD-2.8B (ST7701 RGB, 480×640 en vertical, GT911) y ESP32-S3-Touch-LCD-1.69 (ST7789V2, 240×280, CST816T). Un entorno de PlatformIO por placa; capa de placa común (`src/board.h`, `src/board_common.*`); la CI y la release compilan las cuatro. Sin probar en las placas: `docs/HARDWARE.md`.
- La interfaz se adapta a la pantalla: fuentes, pestañas (texto a la izquierda o iconos abajo), columnas de las tablas repartidas por el ancho real, botones a la medida del texto, pantalla de arranque pequeña para menos de 600 líneas. Al abrir el teclado las pestañas se encogen por encima y el campo se pone a la vista; ✓ y el botón de cerrar lo esconden.
- **LVGL 9.6** (antes 8.4): pantalla, táctil y tick con la API nueva; ventana de cambio de modo con `lv_msgbox` de LVGL 9; icono de la barra en ARGB8888 (LVGL 9 no pinta ARGB8565). NimBLE-Arduino 2.5.2.
- Revisión: la lista de modos ya no se reinicia con cada evento del tracker (borraba la elección antes de pulsar «Switch mode»); columnas de Heard, Messages y DAPNET y la pestaña SAT ajustadas al ancho real (834 px), sin barras de desplazamiento; el PIN debe tener 6 cifras; la dirección BLE se compara sin mayúsculas; acceso con cerrojo a la dirección y al nombre del tracker desde las dos tareas.
- **Pantalla negra arreglada.** El expansor de E/S usaba los bits desplazados uno (EXIOn es el bit n), así que el LCD se quedaba sin alimentación (EXIO6) y se seleccionaba CAN. El PWM de la luz de fondo va al revés (0 = máximo) y se escribía al derecho: al 100 % quedaba apagada. Comprobado con el código de ejemplo de Waveshare. Al arrancar se listan por el puerto serie los dispositivos I2C; el táctil admite también la dirección 0x14.
- Pestaña Sensors: tiempo y calidad del aire de internet que la app manda al tracker (OpenWeatherMap / IQAir; campo `net` del evento `sensors` del tracker), y el origen del QNH (GPS, manual o internet).
- Pantalla de arranque: la de bienvenida de la app Android (logo sobre #131A22 con fundido, «Display» + versión debajo; 2,2 s y pasa con fundido a la principal). Icono de la app en la barra superior. Imágenes en `src/ui_assets.cpp` (`tools/make_ui_assets.py`, fuentes en `tools/assets/`).

## 5.3.5 — 2026-11-08
- Primera versión: pantalla BLE para el tracker APRS LoRa+ en la Waveshare ESP32-S3-Touch-LCD-7B. Pestañas Status, Heard, Messages, Sensors, SAT / Rotor, Mirror, DAPNET y Settings. Protocolo del tracker comprobado en el PC (34 pruebas). Instalador web (lanzador `display/` del instalador del tracker). Sin probar en la placa.

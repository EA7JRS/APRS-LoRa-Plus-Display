# Changelog

## Sin publicar
- **LVGL 9.6** (antes 8.4): pantalla, táctil y tick con la API nueva; ventana de cambio de modo con `lv_msgbox` de LVGL 9; icono de la barra en ARGB8888 (LVGL 9 no pinta ARGB8565). NimBLE-Arduino 2.5.2.
- Revisión: la lista de modos ya no se reinicia con cada evento del tracker (borraba la elección antes de pulsar «Switch mode»); columnas de Heard, Messages y DAPNET y la pestaña SAT ajustadas al ancho real (834 px), sin barras de desplazamiento; el PIN debe tener 6 cifras; la dirección BLE se compara sin mayúsculas; acceso con cerrojo a la dirección y al nombre del tracker desde las dos tareas.
- **Pantalla negra arreglada.** El expansor de E/S usaba los bits desplazados uno (EXIOn es el bit n), así que el LCD se quedaba sin alimentación (EXIO6) y se seleccionaba CAN. El PWM de la luz de fondo va al revés (0 = máximo) y se escribía al derecho: al 100 % quedaba apagada. Comprobado con el código de ejemplo de Waveshare. Al arrancar se listan por el puerto serie los dispositivos I2C; el táctil admite también la dirección 0x14.
- Pestaña Sensors: tiempo y calidad del aire de internet que la app manda al tracker (OpenWeatherMap / IQAir; campo `net` del evento `sensors` del tracker), y el origen del QNH (GPS, manual o internet).
- Pantalla de arranque: la de bienvenida de la app Android (logo sobre #131A22 con fundido, «Display» + versión debajo; 2,2 s y pasa con fundido a la principal). Icono de la app en la barra superior. Imágenes en `src/ui_assets.cpp` (`tools/make_ui_assets.py`, fuentes en `tools/assets/`).

## 5.3.5 — 2026-11-08
- Primera versión: pantalla BLE para el tracker APRS LoRa+ en la Waveshare ESP32-S3-Touch-LCD-7B. Pestañas Status, Heard, Messages, Sensors, SAT / Rotor, Mirror, DAPNET y Settings. Protocolo del tracker comprobado en el PC (34 pruebas). Instalador web (lanzador `display/` del instalador del tracker). Sin probar en la placa.

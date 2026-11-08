# Waveshare ESP32-S3-Touch-LCD-7B

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later.

ESP32-S3-WROOM-1 N16R8 (16 MB de flash, 8 MB de PSRAM OPI), panel RGB 1024×600, táctil GT911, Bluetooth 5 (LE) y WiFi.

## Pines (wiki de Waveshare de la 7B)

| Función | Pines |
|---|---|
| Sincronía y reloj | VSYNC 3, HSYNC 46, DE 5, PCLK 7 |
| Rojo R3…R7 | 1, 2, 42, 41, 40 |
| Verde G2…G7 | 39, 0, 45, 48, 47, 21 |
| Azul B3…B7 | 14, 38, 18, 17, 10 |
| Táctil GT911 | SDA 8, SCL 9 (I2C compartido), INT 4, dirección 0x5D |
| Expansor de E/S | I2C 0x24 (mismo bus) |
| Tarjeta TF | MOSI 11, SCK 12, MISO 13, CS por el expansor |
| CAN | TX 20, RX 19 (compartidos con USB) |
| RS485 | RXD 16, TXD 15 |

Expansor: EXIO1 reset del táctil, EXIO2 luz de fondo (DISP), EXIO3 reset del LCD, EXIO4 CS de la TF, EXIO5 selección USB / CAN (alto = CAN), EXIO6 alimentación VDD / VCOM del LCD.

## Sin verificar en la placa (mirar primero si la pantalla no arranca)

1. **Protocolo del expansor.** La 7B lleva un chip «IO extension» de Waveshare en 0x24 (un CH32V003 según un proyecto de la comunidad), **no** el CH422G de la LCD-7. `board_lcd7b.cpp` supone registro 0x02 = modo (0xFF = salidas), 0x03 = salidas, 0x05 = PWM de la luz de fondo, y EXIOn = bit n−1. Es de memoria del código de demostración de Waveshare: comprobarlo con el ejemplo de Arduino de la 7B.
2. **Tiempos del panel.** `hsync 162 / bp 152 / fp 48`, `vsync 45 / bp 13 / fp 3`, `pclk_active_neg = 1` salen de un informe de un usuario (issue #4 de `waveshareteam/ESP32-S3-Touch-LCD-7B`), no del fabricante. El reloj de píxel se fija con `LCD_PCLK_HZ` en `platformio.ini` (18 MHz por defecto; con la PSRAM a 120 MHz se puede subir a 30 MHz para menos parpadeo).
3. **Táctil.** Se supone la misma orientación que el panel (sin intercambiar ejes).
4. **Enlace BLE.** Emparejamiento con PIN por inyección de clave (`onPassKeyEntry`); comprobar con un tracker real.

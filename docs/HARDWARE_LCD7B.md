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

Expansor (CH32V003 con firmware «IO extension» de Waveshare, I2C 0x24): EXIO1 reset del táctil, EXIO2 luz de fondo (DISP), EXIO3 reset del LCD, EXIO4 CS de la TF, EXIO5 selección USB / CAN (alto = CAN), EXIO6 alimentación VDD / VCOM del LCD. **EXIOn = bit n** del registro de salidas.

Registros (comprobados con `io_extension.cpp` de los ejemplos de Arduino de Waveshare): 0x02 modo (0xFF = todo salidas), 0x03 salidas, 0x04 entradas, 0x05 PWM de la luz de fondo, 0x06 ADC (batería). El PWM va **al revés**: 0 = brillo máximo, 255 = apagada (ejemplo `15_LVGL_SLIDER`); Waveshare lo limita a 97 %.

Tiempos del panel (iguales que `rgb_lcd_port.cpp` de Waveshare): `hsync 162 / bp 152 / fp 48`, `vsync 45 / bp 13 / fp 3`, `pclk_active_neg = 1`. Waveshare usa 30 MHz de reloj de píxel; aquí `LCD_PCLK_HZ` = 18 MHz por defecto.

## Sin verificar en la placa (mirar primero si la pantalla no arranca)

1. **Pantalla negra en la primera versión (corregido).** Los bits del expansor iban desplazados uno (EXIOn = bit n−1): el LCD nunca recibía VDD/VCOM (EXIO6) y se ponía el modo CAN. Además el PWM se escribía al derecho, así que al 100 % la luz quedaba apagada.
2. **Reloj de píxel.** Si hay parpadeo o la imagen se desplaza, probar `LCD_PCLK_HZ` entre 16 y 30 MHz.
3. **Táctil.** Se supone la misma orientación que el panel (sin intercambiar ejes).
4. **Enlace BLE.** Emparejamiento con PIN por inyección de clave (`onPassKeyEntry`); comprobar con un tracker real.

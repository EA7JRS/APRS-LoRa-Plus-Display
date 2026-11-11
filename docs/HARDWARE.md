# Placas

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later.

| Entorno (`pio run -e …`) | Placa | Pantalla | Táctil | Serie |
|---|---|---|---|---|
| `waveshare_lcd7b` | ESP32-S3-Touch-LCD-7B | 7", RGB 1024×600 | GT911 | UART0 (CH343) |
| `waveshare_lcd35` | ESP32-S3-Touch-LCD-3.5 y 3.5-C | 3,5", ST7796 SPI, 480×320 en horizontal | FT6336 | USB CDC |
| `waveshare_lcd28b` | ESP32-S3-Touch-LCD-2.8B | 2,8", ST7701 RGB, 480×640 en vertical | GT911 | USB CDC |
| `waveshare_lcd169` | ESP32-S3-Touch-LCD-1.69 | 1,69", ST7789V2 SPI, 240×280 | CST816T | USB CDC |

Todas llevan ESP32-S3 con 16 MB de flash y 8 MB de PSRAM OPI. La interfaz se adapta al tamaño: grande (pestañas con texto a la izquierda), media (480×320 con texto a la izquierda, 480×640 con iconos abajo) y pequeña (iconos abajo, menos columnas en las tablas). Cada placa vive en `src/board_<placa>.cpp`; lo común (TCA9554, GT911, FT6336/CST816, panel SPI con DMA y panel RGB) en `src/board_common.*`.

# Waveshare ESP32-S3-Touch-LCD-7B

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

# Waveshare ESP32-S3-Touch-LCD-7 (sin B, rev 1.2)

Entornos `waveshare_lcd7` y `waveshare_lcd7_v2`. 800×480 RGB, GT911 (0x5D), 16 MB de flash, PSRAM de 8 MB. **No es compatible con la 7B.** Fuente: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7 y `waveshareteam/ESP32-S3-Touch-LCD-7`. Probada el 2026-11-11.

- I2C: SDA 8, SCL 9. Táctil: INT GPIO4, reset por EXIO1.
- RGB: VSYNC 3, HSYNC 46, DE 5, PCLK 7 (16 MHz, flanco de bajada); datos B3-B7 14, 38, 18, 17, 10; G2-G7 39, 0, 45, 48, 47, 21; R3-R7 1, 2, 42, 41, 40.
- Expansor CH422G: sin registros. Un byte a la dirección I2C 0x24 fija el modo (0x01 = salidas) y un byte a 0x38 las salidas EXIO0-7. EXIO1 reset del táctil, EXIO2 luz (encendida / apagada, sin regulación), EXIO3 reset del LCD, EXIO4 CS de la microSD, EXIO5 USB (bajo) / CAN (alto).
- Otros: RS485 en GPIO 15 (TX) / 16 (RX), CAN en 20 / 19 (las patillas del USB nativo), microSD en 11 (MOSI), 12 (SCK), 13 (MISO); `Serial` por el USB-C UART (CH343).

# Waveshare ESP32-S3-Touch-LCD-3.5 / 3.5-C

La 3.5-C es la misma placa con caja y cámara OV5640. Fuente: código de Waveshare (`waveshareteam/ESP32-S3-Touch-LCD-3.5`, ejemplos de ESP-IDF y Arduino).

| Función | Pines |
|---|---|
| LCD ST7796 (SPI) | SCLK 5, MOSI 1, DC 3, CS a masa; reset por el TCA9554 (P1) |
| Luz de fondo | GPIO6 (PWM) |
| I2C | SDA 8, SCL 7: TCA9554 0x20, AXP2101 0x34, FT6336 0x38, ES8311, PCF85063, QMI8658 |

- Panel en vertical 320×480; aquí se usa en horizontal (MADCTL `0x68`, el giro 1 de Arduino_GFX) y el táctil se gira igual (x = y del FT6336, y = 319 − x).
- Secuencia del ST7796: la de `esp_lcd_st7796.c` de Waveshare, con colores invertidos (IPS) y orden BGR.
- El AXP2101 se deja como viene (el ejemplo de Arduino de Waveshare tampoco lo toca).

Sin verificar en la placa: **el giro**. Si la imagen o el táctil salen al revés, compilar con `-DLCD35_ROTATION=3` (MADCTL `0xA8`, táctil al contrario). Reloj SPI a 40 MHz (`LCD_SPI_HZ`; Waveshare usa 80).

# Waveshare ESP32-S3-Touch-LCD-2.8B

Waveshare no publica el código en GitHub. Fuente: el driver de FatihErtugral (`esp32s3-waveshare-2.8-touch-lcd`, sacado del de Waveshare) y la guía de ESPHome de bundoon (`Waveshare-ESP32-S3Touch-LCD-2.8B-Display-Working-with-ESPHome`).

| Función | Pines |
|---|---|
| Sincronía y reloj | HSYNC 38, VSYNC 39, DE 40, PCLK 41 |
| Azul B0…B4 | 5, 45, 48, 47, 21 |
| Verde G0…G5 | 14, 13, 12, 11, 10, 9 |
| Rojo R0…R4 | 46, 3, 8, 18, 17 |
| SPI de configuración del ST7701 (3 hilos, 9 bits) | SCK 2, SDA 1, CS por el TCA9554 (P2) |
| Táctil GT911 | I2C SDA 15, SCL 7, INT 16, reset por el TCA9554 (P1) |
| TCA9554 (0x20) | P0 reset LCD, P1 reset táctil, P2 CS LCD, P3 alimentación TF, P7 zumbador |
| Luz de fondo | GPIO6 (PWM) |

- Tiempos: `hsync 8 / bp 10 / fp 50`, `vsync 2 / bp 18 / fp 8`, reloj por flanco de subida. Reloj de píxel a 18 MHz (`LCD_PCLK_HZ`; Waveshare 30, ESPHome estable a 16).
- La secuencia del ST7701 se manda por SPI hecho por software (GPIO1/2 son también los de la TF, que no se usa).

Sin verificar en la placa: todo lo anterior (no es código del fabricante). Si la imagen sale con colores cambiados o desplazada, mirar primero el orden de los pines de datos y `LCD_PCLK_HZ`.

# Waveshare ESP32-S3-Touch-LCD-1.69

Fuente: wiki de Waveshare (docs.waveshare.com/ESP32-S3-Touch-LCD-1.69) y ejemplos de `waveshareteam/ESP32-S3-Touch-LCD-1.69`.

| Función | Pines |
|---|---|
| LCD ST7789V2 (SPI) | SCLK 6, MOSI 7, CS 5, DC 4, RST 8 |
| Luz de fondo | GPIO15 (PWM) |
| Táctil CST816T (0x15) | SDA 11, SCL 10, RST 13, INT 14 |
| Alimentación | SYS_EN GPIO41 (mantiene la batería), SYS_OUT GPIO40 (botón PWR) |
| Otros | QMI8658 0x6B, PCF85063 0x51, zumbador GPIO42, batería GPIO1 (× 3) |

- La RAM del ST7789 es de 240×320: la ventana se desplaza 20 líneas. Secuencia de Arduino_ST7789 con colores invertidos (IPS).
- Al arrancar se pone SYS_EN alto para que no se apague al soltar PWR con batería.
- Al CST816T se le quita el modo de dormir solo (registro 0xFE = 1); si no contesta se toma como «soltado».

Sin verificar en la placa: el nivel de SYS_EN (la wiki solo dice que mantiene la alimentación) y la orientación del táctil.


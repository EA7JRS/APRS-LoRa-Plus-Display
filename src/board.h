/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Placa de la pantalla: una por entorno de PlatformIO (-DBOARD_...). Cada placa vive en src/board_<placa>.cpp;
 * lo comun (expansor TCA9554, tactiles GT911 / FT6336 / CST816, panel SPI con DMA, panel RGB) en src/board_common.*.
 * Pines y lo que esta sin verificar de cada placa: docs/HARDWARE.md.
 */
#pragma once
#include <Arduino.h>

#if defined(BOARD_LCD7B)          // Waveshare ESP32-S3-Touch-LCD-7B: 7", RGB 1024x600, GT911
#define BOARD_ENV "waveshare_lcd7b"
#define BOARD_W 1024
#define BOARD_H 600
#elif defined(BOARD_LCD35)        // Waveshare ESP32-S3-Touch-LCD-3.5 / 3.5-C: 3,5", ST7796 SPI 320x480 en horizontal, FT6336
#define BOARD_ENV "waveshare_lcd35"
#define BOARD_W 480
#define BOARD_H 320
#elif defined(BOARD_LCD28B)       // Waveshare ESP32-S3-Touch-LCD-2.8B: 2,8", ST7701 RGB 480x640 en vertical, GT911
#define BOARD_ENV "waveshare_lcd28b"
#define BOARD_W 480
#define BOARD_H 640
#elif defined(BOARD_LCD169)       // Waveshare ESP32-S3-Touch-LCD-1.69: 1,69", ST7789V2 SPI 240x280, CST816T
#define BOARD_ENV "waveshare_lcd169"
#define BOARD_W 240
#define BOARD_H 280
#else
#error "Falta la placa: -DBOARD_LCD7B, -DBOARD_LCD35, -DBOARD_LCD28B o -DBOARD_LCD169 (ver platformio.ini)"
#endif

namespace board {
constexpr int W = BOARD_W, H = BOARD_H;
constexpr const char* ENV = BOARD_ENV;

// Arranca alimentacion, expansor, panel, tactil y luz de fondo. false si algo falla (el motivo sale por el puerto serie).
bool begin();

// Buffer de dibujo de LVGL (PSRAM en los paneles RGB, RAM interna con DMA en los SPI). Lineas por buffer: drawLines().
void* allocDrawBuffer(size_t bytes);
int drawLines();
// Los paneles SPI quieren el RGB565 con el byte alto primero (LVGL lo da al reves): hay que darle la vuelta antes de draw().
bool swapBytes();

// Vuelca un rectangulo RGB565 al panel. Cuando el buffer ya se puede reutilizar llama a la funcion de setDrawDone()
// (al momento en los RGB; al acabar el DMA en los SPI).
void setDrawDone(void (*cb)());
void draw(int x1, int y1, int x2, int y2, void* pixels);

// Tactil: 1 = dedo puesto (x, y en pixeles de la pantalla), 0 = soltado, -1 = sin dato nuevo.
int touch(int& x, int& y);

// Luz de fondo, 0..100 (0 = apagada).
void setBacklight(int percent);
}

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Waveshare ESP32-S3-Touch-LCD-7B (1024x600, panel RGB, tactil GT911, expansor de E/S por I2C 0x24).
 * Pines segun la wiki de Waveshare; ver docs/HARDWARE_LCD7B.md para lo que esta sin verificar en la placa.
 */
#pragma once
#include <Arduino.h>

namespace board {
constexpr int W = 1024, H = 600;

// Arranca expansor, luz de fondo, panel RGB y tactil. false si algo falla (el motivo sale por el puerto serie).
bool begin();

// Vuelca un rectangulo RGB565 al panel (llamado desde el flush de LVGL).
void draw(int x1, int y1, int x2, int y2, const void* pixels);

// Tactil: 1 = dedo puesto (x, y en pixeles), 0 = soltado, -1 = sin dato nuevo.
int touch(int& x, int& y);

// Luz de fondo, 0..100 (PWM del expansor; 0 = apagada).
void setBacklight(int percent);
}

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Waveshare ESP32-S3-Touch-LCD-1.69: ST7789V2 por SPI (240x280, RAM de 240x320: ventana desplazada 20 lineas), tactil CST816T
 * (0x15), luz de fondo por PWM en GPIO15, SYS_EN (GPIO41) mantiene la alimentacion con bateria.
 * Pines de la wiki de Waveshare; secuencia de arranque de Arduino_ST7789 (ejemplos de waveshareteam/ESP32-S3-Touch-LCD-1.69).
 */
#include "board.h"
#if defined(BOARD_LCD169)
#include "board_common.h"
#include <Wire.h>

#ifndef LCD_SPI_HZ
#define LCD_SPI_HZ 40000000
#endif

namespace board {
using namespace common;

constexpr int PIN_SDA = 11, PIN_SCL = 10, PIN_TP_RST = 13;
constexpr int PIN_SCLK = 6, PIN_MOSI = 7, PIN_CS = 5, PIN_DC = 4, PIN_RST = 8, PIN_BL = 15;
constexpr int PIN_SYS_EN = 41;
constexpr uint8_t CST816 = 0x15;

static const LcdCmd INIT[] = {
    {0x01, 0, {}, 120},                                           // reset por software
    {0x11, 0, {}, 120},
    {0x3A, 1, {0x55}, 0},                                         // RGB565
    {0x36, 1, {0x00}, 0},                                         // vertical, RGB
    {0xB0, 2, {0x00, 0xF0}, 0},                                   // byte alto primero
    {0xB2, 5, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 0},
    {0xB7, 1, {0x35}, 0},
    {0xBB, 1, {0x19}, 0},
    {0xC0, 1, {0x2C}, 0},
    {0xC2, 1, {0x01}, 0},
    {0xC3, 1, {0x12}, 0},
    {0xC4, 1, {0x20}, 0},
    {0xC6, 1, {0x0F}, 0},
    {0xD0, 2, {0xA4, 0xA1}, 0},
    {0xE0, 14, {0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C, 0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21}, 0},
    {0xE1, 14, {0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B, 0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21}, 0},
    {0x21, 0, {}, 0},                                             // colores invertidos (panel IPS)
    {0x13, 0, {}, 10},
    {0x29, 0, {}, 0},
};

static void (*doneCb)() = nullptr;

bool begin() {
    pinMode(PIN_SYS_EN, OUTPUT);                                  // lo primero: que no se apague al soltar PWR con bateria
    digitalWrite(PIN_SYS_EN, HIGH);

    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW); delay(10);
    digitalWrite(PIN_TP_RST, HIGH); delay(50);
    i2cScan();                                                    // deben salir 0x15 (CST816T), 0x51 (RTC) y 0x6B (IMU)
    if (!i2cWrite(CST816, 0xFE, 0x01)) Serial.println("[board] aviso: el tactil CST816T (0x15) no responde");   // sin auto-dormir

    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, HIGH); delay(10);
    digitalWrite(PIN_RST, LOW); delay(10);
    digitalWrite(PIN_RST, HIGH); delay(120);

    pwmBacklightBegin(PIN_BL);
    SpiPanel p = {PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, LCD_SPI_HZ, 0, 20, (size_t)W * drawLines() * 2};
    if (!spiPanelBegin(p, INIT, sizeof INIT / sizeof INIT[0])) return false;
    setBacklight(100);
    Serial.printf("[board] 1.69: ST7789V2 a %u Hz\n", (unsigned)LCD_SPI_HZ);
    return true;
}

void* allocDrawBuffer(size_t bytes) { return heap_caps_aligned_alloc(4, bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL); }
int drawLines() { return 40; }
bool swapBytes() { return true; }
void setDrawDone(void (*cb)()) { doneCb = cb; spiPanelSetDone(cb); }
void draw(int x1, int y1, int x2, int y2, void* px) { spiPanelDraw(x1, y1, x2, y2, px); }

int touch(int& x, int& y) {
    int rx, ry;
    int r = common::ftRead(CST816, rx, ry);
    if (r != 1) return r;
    x = constrain(rx, 0, W - 1);
    y = constrain(ry, 0, H - 1);
    return 1;
}

void setBacklight(int percent) { pwmBacklightSet(PIN_BL, percent); }

}
#endif

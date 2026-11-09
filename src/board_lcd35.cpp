/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Waveshare ESP32-S3-Touch-LCD-3.5 (y la 3.5-C, que es la misma placa con caja y camara): ST7796 por SPI (320x480, aqui en
 * horizontal 480x320), tactil FT6336 (0x38), expansor TCA9554 (0x20; P1 = reset del LCD), luz de fondo por PWM en GPIO6.
 * Pines y secuencia de arranque del codigo de Waveshare (waveshareteam/ESP32-S3-Touch-LCD-3.5).
 */
#include "board.h"
#if defined(BOARD_LCD35)
#include "board_common.h"
#include <Wire.h>

#ifndef LCD_SPI_HZ
#define LCD_SPI_HZ 40000000                                       // Waveshare usa 80 MHz
#endif
// Giro: 1 = horizontal como el giro 1 de Arduino_GFX; 3 = el contrario (si la imagen y el tactil salen al reves)
#ifndef LCD35_ROTATION
#define LCD35_ROTATION 1
#endif

namespace board {
using namespace common;

constexpr int PIN_SDA = 8, PIN_SCL = 7;
constexpr int PIN_SCLK = 5, PIN_MOSI = 1, PIN_DC = 3, PIN_BL = 6;  // CS a masa en la placa
constexpr uint8_t FT6336 = 0x38;
static Tca9554 tca;
constexpr uint8_t TCA_LCD_RST = 1 << 1;

// MADCTL: giro 1 = MX | MV | BGR, giro 3 = MY | MV | BGR (Arduino_ST7796::setRotation)
constexpr uint8_t MADCTL = LCD35_ROTATION == 3 ? 0xA8 : 0x68;

// vendor_specific_init_default de esp_lcd_st7796.c de Waveshare (+ MADCTL)
static const LcdCmd INIT[] = {
    {0x11, 0, {}, 120},
    {0x36, 1, {MADCTL}, 0},
    {0x3A, 1, {0x05}, 0},
    {0xF0, 1, {0xC3}, 0},
    {0xF0, 1, {0x96}, 0},
    {0xB4, 1, {0x01}, 0},
    {0xB7, 1, {0xC6}, 0},
    {0xC0, 2, {0x80, 0x45}, 0},
    {0xC1, 1, {0x13}, 0},
    {0xC2, 1, {0xA7}, 0},
    {0xC5, 1, {0x0A}, 0},
    {0xE8, 8, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 0},
    {0xE0, 14, {0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31}, 0},
    {0xE1, 14, {0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32}, 0},
    {0xF0, 1, {0x3C}, 0},
    {0xF0, 1, {0x69}, 120},
    {0x21, 0, {}, 0},                                             // colores invertidos (panel IPS)
    {0x29, 0, {}, 0},
};

static void (*doneCb)() = nullptr;

bool begin() {
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    delay(10);
    i2cScan();                                                    // deben salir 0x20 (TCA9554), 0x34 (AXP2101) y 0x38 (FT6336)

    if (!tca.begin(TCA_LCD_RST, TCA_LCD_RST)) Serial.println("[board] aviso: el TCA9554 (0x20) no responde");
    delay(10);
    tca.set(TCA_LCD_RST, false); delay(10);                       // reset del LCD (y del tactil): 1 -> 0 -> 1
    tca.set(TCA_LCD_RST, true); delay(200);

    pwmBacklightBegin(PIN_BL);
    SpiPanel p = {PIN_SCLK, PIN_MOSI, -1, PIN_DC, LCD_SPI_HZ, 0, 0, (size_t)W * drawLines() * 2};
    if (!spiPanelBegin(p, INIT, sizeof INIT / sizeof INIT[0])) return false;
    if (!i2cProbe(FT6336)) Serial.println("[board] aviso: el tactil FT6336 (0x38) no responde");
    setBacklight(100);
    Serial.printf("[board] 3.5: ST7796 a %u Hz, giro %d\n", (unsigned)LCD_SPI_HZ, LCD35_ROTATION);
    return true;
}

void* allocDrawBuffer(size_t bytes) { return heap_caps_aligned_alloc(4, bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL); }
int drawLines() { return 40; }
bool swapBytes() { return true; }
void setDrawDone(void (*cb)()) { doneCb = cb; spiPanelSetDone(cb); }
void draw(int x1, int y1, int x2, int y2, void* px) { spiPanelDraw(x1, y1, x2, y2, px); }

int touch(int& x, int& y) {
    int rx, ry;
    int r = ftRead(FT6336, rx, ry);                               // en vertical (320 x 480), como el giro 0
    if (r != 1) return r;
#if LCD35_ROTATION == 3
    x = constrain(479 - ry, 0, W - 1); y = constrain(rx, 0, H - 1);
#else
    x = constrain(ry, 0, W - 1); y = constrain(319 - rx, 0, H - 1);
#endif
    return 1;
}

void setBacklight(int percent) { pwmBacklightSet(PIN_BL, percent); }

}
#endif

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Waveshare ESP32-S3-Touch-LCD-2.8B: ST7701 con bus RGB de 16 bits (480x640 en vertical) que se configura por SPI de 3 hilos
 * (9 bits, SCK GPIO2, SDA GPIO1, CS por el expansor), tactil GT911, expansor TCA9554 (0x20), luz de fondo por PWM en GPIO6.
 * Waveshare no publica el codigo en GitHub: pines, tiempos y secuencia del ST7701 del driver de FatihErtugral
 * (esp32s3-waveshare-2.8-touch-lcd, sacado del de Waveshare) y de la guia de ESPHome de bundoon. Ver docs/HARDWARE.md.
 */
#include "board.h"
#if defined(BOARD_LCD28B)
#include "board_common.h"
#include <Wire.h>

#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ 18000000                                      // Waveshare 30 MHz; ESPHome estable a 16 MHz
#endif

namespace board {
using namespace common;

constexpr int PIN_SDA = 15, PIN_SCL = 7, PIN_TP_INT = 16, PIN_BL = 6;
constexpr int PIN_SPI_SCK = 2, PIN_SPI_SDA = 1;
constexpr int PIN_HSYNC = 38, PIN_VSYNC = 39, PIN_DE = 40, PIN_PCLK = 41;
// data[0..4] = B0..B4, data[5..10] = G0..G5, data[11..15] = R0..R4
constexpr int DATA_PINS[16] = {5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17};

// TCA9554: P0 reset LCD, P1 reset tactil, P2 CS del LCD, P3 alimentacion de la TF, P7 zumbador
static Tca9554 tca;
constexpr uint8_t TCA_LCD_RST = 1 << 0, TCA_TP_RST = 1 << 1, TCA_LCD_CS = 1 << 2, TCA_BUZZER = 1 << 7;

// Secuencia del ST7701 de Waveshare: {orden, n, datos}; n = 0xFF es una pausa de d[0] * 10 ms
struct St7701Cmd { uint8_t cmd, n; uint8_t d[16]; };
static const St7701Cmd ST7701[] = {
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}}, {0xEF, 1, {0x08}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x10}}, {0xC0, 2, {0x4F, 0x00}}, {0xC1, 2, {0x10, 0x02}}, {0xC2, 2, {0x07, 0x02}}, {0xCC, 1, {0x10}},
    {0xB0, 16, {0x00, 0x10, 0x17, 0x0D, 0x11, 0x06, 0x05, 0x08, 0x07, 0x1F, 0x04, 0x11, 0x0E, 0x29, 0x30, 0x1F}},
    {0xB1, 16, {0x00, 0x0D, 0x14, 0x0E, 0x11, 0x06, 0x04, 0x08, 0x08, 0x20, 0x05, 0x13, 0x13, 0x26, 0x30, 0x1F}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x11}},
    {0xB0, 1, {0x65}}, {0xB1, 1, {0x71}}, {0xB2, 1, {0x82}}, {0xB3, 1, {0x80}}, {0xB5, 1, {0x42}}, {0xB7, 1, {0x85}}, {0xB8, 1, {0x20}},
    {0xC0, 1, {0x09}}, {0xC1, 1, {0x78}}, {0xC2, 1, {0x78}}, {0xD0, 1, {0x88}}, {0xEE, 1, {0x42}},
    {0xE0, 3, {0x00, 0x00, 0x02}},
    {0xE1, 11, {0x04, 0xA0, 0x06, 0xA0, 0x05, 0xA0, 0x07, 0xA0, 0x00, 0x44, 0x44}},
    {0xE2, 12, {0}},
    {0xE3, 4, {0x00, 0x00, 0x22, 0x22}}, {0xE4, 2, {0x44, 0x44}},
    {0xE5, 16, {0x0C, 0x90, 0xA0, 0xA0, 0x0E, 0x92, 0xA0, 0xA0, 0x08, 0x8C, 0xA0, 0xA0, 0x0A, 0x8E, 0xA0, 0xA0}},
    {0xE6, 4, {0x00, 0x00, 0x22, 0x22}}, {0xE7, 2, {0x44, 0x44}},
    {0xE8, 16, {0x0D, 0x91, 0xA0, 0xA0, 0x0F, 0x93, 0xA0, 0xA0, 0x09, 0x8D, 0xA0, 0xA0, 0x0B, 0x8F, 0xA0, 0xA0}},
    {0xEB, 7, {0x00, 0x00, 0xE4, 0xE4, 0x44, 0x00, 0x40}},
    {0x36, 1, {0x00}},                                            // MADCTL: vertical, sin giros
    {0xED, 16, {0xFF, 0xF5, 0x47, 0x6F, 0x0B, 0xA1, 0xAB, 0xFF, 0xFF, 0xBA, 0x1A, 0xB0, 0xF6, 0x74, 0x5F, 0xFF}},
    {0xEF, 6, {0x08, 0x08, 0x08, 0x40, 0x3F, 0x64}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}}, {0xE6, 2, {0x16, 0x7C}}, {0xE8, 2, {0x00, 0x0E}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}},
    {0x11, 1, {0x00}}, {0x00, 0xFF, {20}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}}, {0xE8, 2, {0x00, 0x0C}}, {0x00, 0xFF, {15}}, {0xE8, 2, {0x00, 0x00}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}},
    {0x29, 1, {0x00}}, {0x35, 1, {0x00}},
    {0x11, 1, {0x00}}, {0x00, 0xFF, {20}},
    {0x29, 1, {0x00}}, {0x29, 1, {0x00}}, {0x00, 0xFF, {10}},
};

// SPI de 3 hilos por software: bit D/C (0 orden, 1 dato) y 8 bits, el mas alto primero; se lee en el flanco de subida
static void spi9(bool data, uint8_t v) {
    uint16_t w = (data ? 0x100 : 0) | v;
    for (int i = 8; i >= 0; i--) {
        digitalWrite(PIN_SPI_SCK, LOW);
        digitalWrite(PIN_SPI_SDA, (w >> i) & 1);
        delayMicroseconds(1);
        digitalWrite(PIN_SPI_SCK, HIGH);
        delayMicroseconds(1);
    }
    digitalWrite(PIN_SPI_SCK, LOW);
}

static void st7701Init() {
    pinMode(PIN_SPI_SCK, OUTPUT); pinMode(PIN_SPI_SDA, OUTPUT);
    digitalWrite(PIN_SPI_SCK, LOW);
    tca.set(TCA_LCD_CS, false); delay(10);
    for (const St7701Cmd& c : ST7701) {
        if (c.n == 0xFF) { delay(c.d[0] * 10); continue; }
        spi9(false, c.cmd);
        for (int i = 0; i < c.n; i++) spi9(true, c.d[i]);
    }
}

static uint8_t gtAddr = 0x5D;
static void (*doneCb)() = nullptr;

bool begin() {
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    delay(10);
    i2cScan();                                                    // deben salir 0x20 (TCA9554), 0x5D o 0x14 (GT911), 0x51, 0x6B

    // todo salidas; LCD y tactil sin reset, CS del LCD alto, TF sin alimentar, zumbador callado
    if (!tca.begin(0xFF, TCA_LCD_RST | TCA_TP_RST | TCA_LCD_CS)) { Serial.println("[board] el TCA9554 (0x20) no responde"); return false; }

    // Reset del GT911 con INT bajo: direccion 0x5D
    pinMode(PIN_TP_INT, OUTPUT); digitalWrite(PIN_TP_INT, LOW);
    tca.set(TCA_TP_RST, false); delay(20);
    tca.set(TCA_TP_RST, true); delay(100);
    pinMode(PIN_TP_INT, INPUT);
    if (!i2cProbe(0x5D)) {
        if (i2cProbe(0x14)) gtAddr = 0x14;
        else Serial.println("[board] aviso: el tactil GT911 no responde");
    }

    tca.set(TCA_LCD_RST, false); delay(10);
    tca.set(TCA_LCD_RST, true); delay(120);
    st7701Init();

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.timings.pclk_hz = LCD_PCLK_HZ;
    cfg.timings.h_res = W;
    cfg.timings.v_res = H;
    cfg.timings.hsync_pulse_width = 8;
    cfg.timings.hsync_back_porch = 10;
    cfg.timings.hsync_front_porch = 50;
    cfg.timings.vsync_pulse_width = 2;
    cfg.timings.vsync_back_porch = 18;
    cfg.timings.vsync_front_porch = 8;
    cfg.timings.flags.pclk_active_neg = 0;
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 1;
    cfg.bounce_buffer_size_px = W * 10;
    cfg.hsync_gpio_num = PIN_HSYNC;
    cfg.vsync_gpio_num = PIN_VSYNC;
    cfg.de_gpio_num = PIN_DE;
    cfg.pclk_gpio_num = PIN_PCLK;
    cfg.disp_gpio_num = -1;
    for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = DATA_PINS[i];
    cfg.flags.fb_in_psram = 1;
    bool ok = rgbPanelBegin(cfg);
    tca.set(TCA_LCD_CS, true);                                    // CS arriba despues de montar el panel RGB, como Waveshare
    if (!ok) return false;

    pwmBacklightBegin(PIN_BL);
    setBacklight(100);
    Serial.printf("[board] 2.8B: panel listo (pclk %u Hz), tactil en 0x%02X\n", (unsigned)LCD_PCLK_HZ, gtAddr);
    return true;
}

void* allocDrawBuffer(size_t bytes) { return heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM); }
int drawLines() { return 64; }
bool swapBytes() { return false; }
void setDrawDone(void (*cb)()) { doneCb = cb; }
void draw(int x1, int y1, int x2, int y2, void* px) {
    rgbPanelDraw(x1, y1, x2, y2, px);
    if (doneCb) doneCb();
}
int touch(int& x, int& y) { return gt911Read(gtAddr, x, y, W, H); }
void setBacklight(int percent) { pwmBacklightSet(PIN_BL, percent); }

}
#endif

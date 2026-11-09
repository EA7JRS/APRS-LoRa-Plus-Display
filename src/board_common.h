/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Piezas comunes a las placas: I2C, expansor TCA9554, tactiles, panel SPI (ST7789 / ST7796) con DMA y panel RGB.
 */
#pragma once
#include <Arduino.h>
#include <esp_lcd_panel_rgb.h>

namespace board {
namespace common {

// ---- I2C ----
bool i2cProbe(uint8_t addr);
bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t val);
bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* d, size_t n);
void i2cScan();                                   // lista por el puerto serie lo que responde (diagnostico)

// ---- Expansor TCA9554 (0x20): registro 1 salidas, 3 configuracion (0 = salida) ----
struct Tca9554 {
    uint8_t addr = 0x20, out = 0xFF;
    bool begin(uint8_t outputsMask, uint8_t initial);
    bool set(uint8_t mask, bool on);
};

// ---- Tactiles ----
// GT911 (0x5D o 0x14): registro 0x814E estado, 0x814F primer punto.
int gt911Read(uint8_t addr, int& x, int& y, int w, int h);
// FT6336 (0x38) y CST816 (0x15) comparten el mapa: 0x02 dedos, 0x03..0x06 X alto (4 bits), X bajo, Y alto, Y bajo.
int ftRead(uint8_t addr, int& x, int& y);

// ---- Luz de fondo por PWM (LEDC) en un GPIO; mas duty = mas luz ----
void pwmBacklightBegin(int pin);
void pwmBacklightSet(int pin, int percent);

// ---- Panel SPI MIPI-DCS (ST7789, ST7796) con esp_lcd: ordenes bloqueantes y pixeles por DMA ----
struct LcdCmd { uint8_t cmd, len; uint8_t d[16]; uint16_t ms; };
struct SpiPanel {
    int sclk, mosi, cs, dc;
    uint32_t pclk;
    int xOff, yOff;                               // desplazamiento de la ventana (1,69": la RAM es de 240x320)
    size_t maxTx;                                 // bytes por transferencia (el buffer de LVGL)
};
bool spiPanelBegin(const SpiPanel& p, const LcdCmd* cmds, size_t n);
void spiPanelSetDone(void (*cb)());           // se llama (desde la interrupcion del DMA) al acabar cada spiPanelDraw()
void spiPanelDraw(int x1, int y1, int x2, int y2, const void* px);

// ---- Panel RGB 16 bits (esp_lcd), comun a la 7B y la 2.8B ----
bool rgbPanelBegin(const esp_lcd_rgb_panel_config_t& cfg);
void rgbPanelDraw(int x1, int y1, int x2, int y2, const void* px);

}
}

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "board_common.h"
#include <Wire.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

namespace board {
namespace common {

// ---------------- I2C ----------------
bool i2cProbe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}
bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}
bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* d, size_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; i++) d[i] = Wire.read();
    return true;
}
void i2cScan() {
    Serial.print("[board] I2C:");
    for (uint8_t a = 1; a < 127; a++)
        if (i2cProbe(a)) Serial.printf(" 0x%02X", a);
    Serial.println();
}

// ---------------- TCA9554 ----------------
bool Tca9554::begin(uint8_t outputsMask, uint8_t initial) {
    out = initial;
    if (!i2cWrite(addr, 0x01, out)) return false;                 // primero el nivel, luego la direccion: sin pulsos
    return i2cWrite(addr, 0x03, (uint8_t)~outputsMask);           // 0 = salida
}
bool Tca9554::set(uint8_t mask, bool on) {
    if (on) out |= mask; else out &= ~mask;
    return i2cWrite(addr, 0x01, out);
}

// ---------------- Tactiles ----------------
int gt911Read(uint8_t addr, int& x, int& y, int w, int h) {
    auto rd = [&](uint16_t reg, uint8_t* d, size_t n) {
        Wire.beginTransmission(addr);
        Wire.write(reg >> 8); Wire.write(reg & 0xFF);
        if (Wire.endTransmission(false) != 0) return false;
        if (Wire.requestFrom((int)addr, (int)n) != (int)n) return false;
        for (size_t i = 0; i < n; i++) d[i] = Wire.read();
        return true;
    };
    uint8_t st = 0;
    if (!rd(0x814E, &st, 1)) return -1;
    if (!(st & 0x80)) return -1;                                  // sin dato nuevo: LVGL conserva el ultimo estado
    uint8_t n = st & 0x0F;
    int res = 0;
    if (n >= 1 && n <= 5) {
        uint8_t p[6];
        if (rd(0x814F, p, 6)) {
            x = constrain(p[1] | (p[2] << 8), 0, w - 1);
            y = constrain(p[3] | (p[4] << 8), 0, h - 1);
            res = 1;
        }
    }
    Wire.beginTransmission(addr);                                 // borrar el estado para el siguiente punto
    Wire.write(0x81); Wire.write(0x4E); Wire.write(0);
    Wire.endTransmission();
    return res;
}

int ftRead(uint8_t addr, int& x, int& y) {
    uint8_t d[5];
    // sin respuesta = soltado (el CST816 deja de contestar al dormirse; asi no se queda un dedo "puesto")
    if (!i2cRead(addr, 0x02, d, 5)) return 0;
    uint8_t n = d[0] & 0x0F;
    if (n == 0 || n > 5) return 0;
    x = ((d[1] & 0x0F) << 8) | d[2];
    y = ((d[3] & 0x0F) << 8) | d[4];
    return 1;
}

// ---------------- Luz de fondo PWM ----------------
void pwmBacklightBegin(int pin) {
    ledcAttach(pin, 5000, 10);
    ledcWrite(pin, 0);
}
void pwmBacklightSet(int pin, int percent) {
    percent = constrain(percent, 0, 100);
    ledcWrite(pin, (uint32_t)percent * 1023 / 100);
}

// ---------------- Panel SPI ----------------
static esp_lcd_panel_io_handle_t spiIo = nullptr;
static int spiXOff = 0, spiYOff = 0;
static void (*spiDone)() = nullptr;

static bool onColorDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
    if (spiDone) spiDone();
    return false;
}

bool spiPanelBegin(const SpiPanel& p, const LcdCmd* cmds, size_t n) {
    spi_bus_config_t bus = {};
    bus.sclk_io_num = p.sclk;
    bus.mosi_io_num = p.mosi;
    bus.miso_io_num = -1;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = (int)p.maxTx;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) { Serial.println("[board] spi_bus_initialize ha fallado"); return false; }
    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num = p.cs;
    io.dc_gpio_num = p.dc;
    io.spi_mode = 0;
    io.pclk_hz = p.pclk;
    io.trans_queue_depth = 10;
    io.on_color_trans_done = onColorDone;
    io.lcd_cmd_bits = 8;
    io.lcd_param_bits = 8;
    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &spiIo) != ESP_OK) { Serial.println("[board] esp_lcd_new_panel_io_spi ha fallado"); return false; }
    spiXOff = p.xOff;
    spiYOff = p.yOff;
    for (size_t i = 0; i < n; i++) {
        esp_lcd_panel_io_tx_param(spiIo, cmds[i].cmd, cmds[i].len ? cmds[i].d : nullptr, cmds[i].len);
        if (cmds[i].ms) delay(cmds[i].ms);
    }
    return true;
}
void spiPanelSetDone(void (*cb)()) { spiDone = cb; }

void spiPanelDraw(int x1, int y1, int x2, int y2, const void* px) {
    if (!spiIo) { if (spiDone) spiDone(); return; }
    x1 += spiXOff; x2 += spiXOff; y1 += spiYOff; y2 += spiYOff;
    const uint8_t ca[4] = {(uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2};
    const uint8_t ra[4] = {(uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2};
    esp_lcd_panel_io_tx_param(spiIo, 0x2A, ca, 4);                // CASET (espera a que acabe el DMA anterior)
    esp_lcd_panel_io_tx_param(spiIo, 0x2B, ra, 4);                // RASET
    esp_lcd_panel_io_tx_color(spiIo, 0x2C, px, (size_t)(x2 - x1 + 1) * (y2 - y1 + 1) * 2);   // RAMWR por DMA
}

// ---------------- Panel RGB ----------------
static esp_lcd_panel_handle_t rgb = nullptr;

bool rgbPanelBegin(const esp_lcd_rgb_panel_config_t& cfg) {
    if (esp_lcd_new_rgb_panel(&cfg, &rgb) != ESP_OK) { Serial.println("[board] esp_lcd_new_rgb_panel ha fallado"); return false; }
    esp_lcd_panel_reset(rgb);
    esp_lcd_panel_init(rgb);
    return true;
}
void rgbPanelDraw(int x1, int y1, int x2, int y2, const void* px) {
    if (rgb) esp_lcd_panel_draw_bitmap(rgb, x1, y1, x2 + 1, y2 + 1, px);
}

}
}

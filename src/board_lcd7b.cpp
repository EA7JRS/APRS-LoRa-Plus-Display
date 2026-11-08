/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "board_lcd7b.h"
#include <Wire.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ 18000000
#endif

namespace board {

// ---- Pines (wiki de Waveshare, ESP32-S3-Touch-LCD-7B) ----
constexpr int PIN_SDA = 8, PIN_SCL = 9, PIN_TP_INT = 4;
constexpr int PIN_VSYNC = 3, PIN_HSYNC = 46, PIN_DE = 5, PIN_PCLK = 7;
// data[0..4] = B3..B7, data[5..10] = G2..G7, data[11..15] = R3..R7
constexpr int DATA_PINS[16] = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40};

// ---- Expansor de E/S (chip "IO extension" de Waveshare en 0x24) ----
// SIN VERIFICAR en la placa: registros 0x02 modo, 0x03 salidas, 0x05 PWM de la luz de fondo, y EXIOn = bit (n-1).
// Asignacion segun la wiki: EXIO1 reset tactil, EXIO2 luz de fondo (DISP), EXIO3 reset LCD, EXIO4 CS de la TF,
// EXIO5 USB/CAN (alto = CAN), EXIO6 alimentacion VDD/VCOM del LCD.
constexpr uint8_t IOX_ADDR = 0x24, IOX_REG_MODE = 0x02, IOX_REG_OUT = 0x03, IOX_REG_PWM = 0x05;
constexpr uint8_t EXIO_TP_RST = 1 << 0, EXIO_DISP = 1 << 1, EXIO_LCD_RST = 1 << 2, EXIO_SD_CS = 1 << 3, EXIO_USB_SEL = 1 << 4, EXIO_LCD_VDD = 1 << 5;
static uint8_t ioxOut = 0;

static bool ioxWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(IOX_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool ioxSet(uint8_t mask, bool on) {
    if (on) ioxOut |= mask; else ioxOut &= ~mask;
    return ioxWrite(IOX_REG_OUT, ioxOut);
}

// ---- Tactil GT911 ----
constexpr uint8_t GT_ADDR = 0x5D;
static bool gtRead(uint16_t reg, uint8_t* d, size_t n) {
    Wire.beginTransmission(GT_ADDR);
    Wire.write(reg >> 8); Wire.write(reg & 0xFF);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)GT_ADDR, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; i++) d[i] = Wire.read();
    return true;
}
static void gtWrite(uint16_t reg, uint8_t v) {
    Wire.beginTransmission(GT_ADDR);
    Wire.write(reg >> 8); Wire.write(reg & 0xFF); Wire.write(v);
    Wire.endTransmission();
}

static esp_lcd_panel_handle_t panel = nullptr;

bool begin() {
    Wire.begin(PIN_SDA, PIN_SCL, 400000);

    // Expansor: todo salidas; LCD apagado y en reset, tactil en reset, USB (no CAN) seleccionado
    if (!ioxWrite(IOX_REG_MODE, 0xFF)) { Serial.println("[board] el expansor (0x24) no responde"); return false; }
    ioxOut = 0;
    ioxWrite(IOX_REG_OUT, ioxOut);
    delay(10);

    // Reset del GT911 con INT bajo: direccion 0x5D
    pinMode(PIN_TP_INT, OUTPUT); digitalWrite(PIN_TP_INT, LOW);
    ioxSet(EXIO_TP_RST, false); delay(10);
    ioxSet(EXIO_TP_RST, true); delay(10);
    pinMode(PIN_TP_INT, INPUT); delay(50);

    // Alimentacion y reset del LCD
    ioxSet(EXIO_LCD_VDD, true); delay(10);
    ioxSet(EXIO_LCD_RST, false); delay(10);
    ioxSet(EXIO_LCD_RST, true); delay(120);

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.timings.pclk_hz = LCD_PCLK_HZ;
    cfg.timings.h_res = W;
    cfg.timings.v_res = H;
    // Tiempos medidos por un usuario de la placa (issue #4 de waveshareteam/ESP32-S3-Touch-LCD-7B); no son del fabricante
    cfg.timings.hsync_pulse_width = 162;
    cfg.timings.hsync_back_porch = 152;
    cfg.timings.hsync_front_porch = 48;
    cfg.timings.vsync_pulse_width = 45;
    cfg.timings.vsync_back_porch = 13;
    cfg.timings.vsync_front_porch = 3;
    cfg.timings.flags.pclk_active_neg = 1;
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
    if (esp_lcd_new_rgb_panel(&cfg, &panel) != ESP_OK) { Serial.println("[board] esp_lcd_new_rgb_panel ha fallado"); return false; }
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);

    setBacklight(100);
    Serial.println("[board] panel y tactil listos");
    return true;
}

void draw(int x1, int y1, int x2, int y2, const void* pixels) {
    if (panel) esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, pixels);
}

int touch(int& x, int& y) {
    uint8_t st = 0;
    if (!gtRead(0x814E, &st, 1)) return -1;
    if (!(st & 0x80)) return -1;                      // sin dato nuevo: LVGL conserva el ultimo estado
    uint8_t n = st & 0x0F;
    int res = 0;
    if (n >= 1 && n <= 5) {
        uint8_t p[6];
        if (gtRead(0x814F, p, 6)) { x = constrain(p[1] | (p[2] << 8), 0, W - 1); y = constrain(p[3] | (p[4] << 8), 0, H - 1); res = 1; }
    }
    gtWrite(0x814E, 0);
    return res;
}

void setBacklight(int percent) {
    percent = constrain(percent, 0, 100);
    ioxSet(EXIO_DISP, percent > 0);
    ioxWrite(IOX_REG_PWM, (uint8_t)((percent * 255) / 100));
}

} // namespace board

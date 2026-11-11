/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * APRS LoRa+ Display: pantalla tactil (Waveshare ESP32-S3-Touch-LCD-7B, -3.5/3.5-C, -2.8B o -1.69) que se conecta por BLE
 * al tracker APRS LoRa+. La placa la elige el entorno de PlatformIO (src/board.h).
 */
#include <Arduino.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <Preferences.h>
#include <lvgl.h>
#include "board.h"
#include "protocol.h"
#include "tracker_link.h"
#include "ui.h"
#ifdef UI_RADIO
#include "net.h"
#include "settings.h"
#endif

// LVGL 9 dibuja cada nivel de objetos con dos funciones que se llaman entre si (refr_obj / lv_obj_redraw)
// y con las pestanas anidadas no cabe en los 8 KB de serie de loop(): reinicio en bucle con
// "Stack canary watchpoint triggered (loopTask)" (7B, 5.4.1). 32 KB salen de la RAM interna.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

static proto::Model model;
static lv_display_t* disp = nullptr;

// La placa avisa cuando ya se puede reutilizar el buffer (al momento en los paneles RGB, al acabar el DMA en los SPI)
static void drawDone() { lv_display_flush_ready(disp); }

// Paleta elegida en Settings: se guarda y se arranca de nuevo para montar la interfaz con ella
static int loadTheme() {
    Preferences p;
    p.begin("display", true);
    int t = p.getInt("theme", ui::THEME_APP);
    p.end();
    return t;
}
static void saveTheme(int t) {
#ifndef UI_RADIO                                        // la 7B v2 guarda la suya en sus ajustes (settings.cpp)
    Preferences p;
    p.begin("display", false);
    p.putInt("theme", t);
    p.end();
#endif
    Serial.printf("[main] paleta %d: reinicio\n", t);
    delay(100);
    ESP.restart();
}

static void flush(lv_display_t*, const lv_area_t* a, uint8_t* px) {
    if (board::swapBytes()) lv_draw_rgb565_swap(px, (uint32_t)lv_area_get_width(a) * lv_area_get_height(a));
    board::draw(a->x1, a->y1, a->x2, a->y2, px);
}

static void readTouch(lv_indev_t*, lv_indev_data_t* data) {
    static int lx = 0, ly = 0;
    static bool pressed = false;
    int x, y;
    int r = board::touch(x, y);
    if (r == 1) { lx = x; ly = y; pressed = true; }
    else if (r == 0) pressed = false;
    data->point.x = lx; data->point.y = ly;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static uint32_t tickMs() { return millis(); }

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== APRS LoRa+ Display " DISPLAY_VERSION " (" BOARD_ENV ") ===");
    bool ok = board::begin();

    lv_init();
    lv_tick_set_cb(tickMs);

    // Dos buffers parciales (LVGL dibuja en uno mientras el otro va al panel)
    const uint32_t bytes = (uint32_t)board::W * board::drawLines() * 2;
    void* b1 = board::allocDrawBuffer(bytes);
    void* b2 = board::allocDrawBuffer(bytes);
    if (!b1 || !b2) Serial.println("[main] sin memoria para los buffers de LVGL");
    disp = lv_display_create(board::W, board::H);
    board::setDrawDone(drawDone);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, readTouch);

    trk::begin();
#ifdef UI_RADIO
    cfg::load();
    board::setBacklight(cfg::get().backlight);
    net::setModel(&model);
    net::begin();
#endif
    ui::begin(&model, loadTheme(), saveTheme);
    if (!ok) Serial.println("La placa no ha arrancado bien: revisar docs/HARDWARE.md");
}

// Peticiones periodicas al tracker (el resto de datos llega solo como eventos)
static void scheduler() {
    static trk::State last = trk::State::Idle;
    static uint32_t tStatus = 0, tHeard = 0, tMsgs = 0;
    trk::State s = trk::state();
    uint32_t now = millis();
    if (s == trk::State::Linked && last != trk::State::Linked) {
        trk::send(proto::cmdSimple("info"));
        trk::send(proto::cmdSimple("status"));
        trk::send(proto::cmdSub(5, 2));                // sensores cada 5 s y GNSS cada 2 s
        trk::send(proto::cmdHeard(0, 12));
#ifdef UI_RADIO
        trk::send(proto::cmdMsgs(0, 20));
        trk::send(proto::cmdSimple("tgtlist"));
#else
        trk::send(proto::cmdMsgs(0, 10));
#endif
        tStatus = tHeard = tMsgs = now;
    }
    if (s != trk::State::Linked && last == trk::State::Linked) { model.revision++; }
    last = s;
    if (s != trk::State::Linked) return;
    if (now - tStatus > 10000) { tStatus = now; trk::send(proto::cmdSimple("status")); }
#ifdef UI_RADIO
    // la lista entera (hasta 20 estaciones y 60 mensajes en el tracker): paginas de 12 y de 20
    if (now - tHeard > (ui::heardVisible() ? 6000u : 20000u)) {
        tHeard = now;
        trk::send(proto::cmdHeard(0, 12));
        if (model.heardTot > 12) trk::send(proto::cmdHeard(12, 12));
    }
    if (now - tMsgs > (ui::messagesVisible() ? 10000u : 30000u)) {
        tMsgs = now;
        for (int f = 0; f < model.msgTot && f < 60; f += 20) trk::send(proto::cmdMsgs(f, 20));
    }
#else
    if (now - tHeard > (ui::heardVisible() ? 5000u : 20000u)) { tHeard = now; trk::send(proto::cmdHeard(0, 12)); }
    if (now - tMsgs > (ui::messagesVisible() ? 8000u : 30000u)) { tMsgs = now; trk::send(proto::cmdMsgs(0, 10)); }
#endif
}

// Instalador web: por USB contesta a "APRS-INFO?" como el resto de dispositivos APRS LoRa+.
static void usbInfo() {
    static char buf[16];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            buf[n] = 0;
            if (n && strcmp(buf, "APRS-INFO?") == 0)
                Serial.println("APRS-LoRa-Plus: device=display board=" BOARD_ENV " version=" DISPLAY_VERSION " chip=ESP32-S3 ota=0");
            n = 0;
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = c;
        } else {
            n = 0;
        }
    }
}

void loop() {
    usbInfo();
    std::string line;
    int n = 0;
    while (n++ < 20 && trk::poll(line)) {
        model.now = millis();
        proto::applyEvent(model, line.c_str());
        // un mensaje nuevo, enviado o con ack cambia el registro: se vuelve a pedir la lista
#ifdef UI_RADIO
        if (line.find("\"e\":\"msg\"") != std::string::npos) for (int f = 0; f < model.msgTot && f < 60; f += 20) trk::send(proto::cmdMsgs(f, 20));
#else
        if (line.find("\"e\":\"msg\"") != std::string::npos) trk::send(proto::cmdMsgs(0, 10));
#endif
    }
    scheduler();
#ifdef UI_RADIO
    net::loop();
#endif
    static uint32_t t = 0;
    if (millis() - t > 300) { t = millis(); ui::update(); }
    lv_timer_handler();
    // pila libre de loop() (lo minimo que ha quedado), cada minuto por el puerto serie
    static uint32_t ts = 0;
    if (millis() - ts > 60000) { ts = millis(); Serial.printf("[main] pila libre: %u bytes\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr)); }
    delay(5);
}

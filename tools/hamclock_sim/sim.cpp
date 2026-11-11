/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * El reloj en el PC: la misma interfaz (src/hamclock/hc_ui.cpp) con LVGL 9.6 de verdad, datos de ejemplo que entran por los
 * mismos lectores de JSON que en la placa y toques simulados. Saca capturas RGB565 de 1024x600 (build.sh las pasa a PNG).
 */
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include "hc_model.h"
#include "hc_ui.h"

static const int W = 1024, H = 600;
static uint16_t fb[W * H], draw[W * H];
static uint32_t ms = 0;
static int tx = 0, ty = 0;
static bool tdown = false;

static void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
    int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++) memcpy(&fb[y * W + a->x1], px + (size_t)(y - a->y1) * w * 2, (size_t)w * 2);
    lv_display_flush_ready(d);
}
static void touch(lv_indev_t*, lv_indev_data_t* d) {
    d->point.x = tx; d->point.y = ty;
    d->state = tdown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static uint32_t tick() { return ms; }
static void run(int n) { for (int i = 0; i < n; i++) { ms += 10; hcui::update(); lv_timer_handler(); } }
static void tap(int x, int y) { tx = x; ty = y; tdown = true; run(8); tdown = false; run(8); }
static void save(const char* name) {
    FILE* f = fopen(name, "wb");
    fwrite(fb, 2, W * H, f);
    fclose(f);
}
template <typename F, typename A> static void feed(hc::Model& m, const std::string& json, F filter, A apply) {
    JsonDocument f, doc;
    filter(f);
    DeserializationError e = deserializeJson(doc, json, DeserializationOption::Filter(f));
    if (e || !apply(m, doc.as<JsonVariantConst>())) printf("JSON de ejemplo rechazado: %s\n", e.c_str());
}
static void onTune(double mhz, const char* mode) { printf("Tune: %.3f MHz %s\n", mhz, mode); }
static void onSave(const hc::Config& c) { printf("Save: %s %s %s\n", c.call, c.grid, c.server); }

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : "/tmp";
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t* disp = lv_display_create(W, H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, draw, nullptr, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t* in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch);

    static hc::Model m;
    hc::Config cfg;
    strcpy(cfg.call, "EA7JRS"); strcpy(cfg.grid, "IM67"); strcpy(cfg.ssid, "casa"); strcpy(cfg.rig, "192.168.1.50");
    feed(m, R"({"sfi":{"current":148},"ssn":{"current":112},"bz":{"current":-2.1},"kp":{"current":2.33,"history":[{"value":1},
        {"value":2},{"value":2.33},{"value":3},{"value":2},{"value":1.33},{"value":2},{"value":2.67},{"value":3},{"value":4.33},
        {"value":3},{"value":2},{"value":2.33},{"value":1},{"value":2},{"value":2.33}]}})", hc::filterSolar, hc::applySolar);
    feed(m, R"({"solarData":{"aIndex":"8","xray":"C1.4","solarWind":"412.3"},"bandConditions":[
        {"name":"80m-40m","time":"day","condition":"Fair"},{"name":"30m-20m","time":"day","condition":"Good"},
        {"name":"17m-15m","time":"day","condition":"Good"},{"name":"12m-10m","time":"day","condition":"Fair"},
        {"name":"80m-40m","time":"night","condition":"Good"},{"name":"30m-20m","time":"night","condition":"Good"},
        {"name":"17m-15m","time":"night","condition":"Fair"},{"name":"12m-10m","time":"night","condition":"Poor"}]})",
         hc::filterN0nbh, hc::applyN0nbh);
    struct S { const char *f, *dx, *de, *c; double la, lo; };
    static const S ss[] = {{"14.074", "JA1XYZ", "EA5ABC", "FT8", 35.7, 139.7}, {"7.155", "VK3QQ", "DL1AA", "", -37.8, 144.9},
        {"21.025", "ZS6ABC", "EA7KK", "CW", -26.2, 28.0}, {"28.495", "PY2XX", "CT1AB", "", -23.5, -46.6},
        {"14.205", "K1ABC", "G4XYZ", "", 42.4, -71.1}, {"18.100", "9K2AA", "IZ2BB", "FT8", 29.4, 47.9},
        {"7.012", "UA9XX", "OH2CC", "CW", 56.8, 60.6}, {"10.136", "LU5DD", "EA1EE", "", -34.6, -58.4},
        {"21.074", "VU2FF", "SV1GG", "", 28.6, 77.2}, {"14.030", "W6HH", "F5II", "CW", 34.0, -118.2},
        {"28.074", "CE3JJ", "EA8KK", "FT8", -33.4, -70.6}, {"7.180", "OX3LL", "LA1MM", "SSB", 64.2, -51.7},
        {"14.250", "A61NN", "I2OO", "", 25.2, 55.3}, {"18.075", "ZL2PP", "VK2QQ", "CW", -41.3, 174.8},
        {"21.290", "5Z4RR", "EA3SS", "SSB", -1.3, 36.8}, {"14.076", "KH6TT", "JA3UU", "", 21.3, -157.8},
        {"50.313", "EA8ZZ", "EA7XX", "FT8", 28.1, -15.4}, {"3.780", "OH8AA", "SM5BB", "", 65.0, 25.5}};
    time_t now = time(nullptr);
    std::string js = "[";
    int i = 0;
    for (const S& s : ss) {
        struct tm tm;
        time_t t = now - i * 47;
        gmtime_r(&t, &tm);
        char b[320];
        snprintf(b, sizeof(b), "%s{\"spotter\":\"%s\",\"dxCall\":\"%s\",\"dxLat\":%g,\"dxLon\":%g,\"freq\":\"%s\",\"comment\":\"%s\","
                 "\"time\":\"%02d%02dz\",\"timestamp\":%lld}", i ? "," : "", s.de, s.dx, s.la, s.lo, s.f, s.c, tm.tm_hour, tm.tm_min,
                 (long long)t * 1000);
        js += b;
        i++;
    }
    feed(m, js + "]", hc::filterSpots, hc::applySpots);
    feed(m, R"([{"activator":"EA4XX","reference":"ES-0123","frequency":"14244","mode":"SSB"},
        {"activator":"DL2YY/P","reference":"DE-0456","frequency":"7144","mode":"SSB"},
        {"activator":"F4AAA","reference":"FR-0789","frequency":"14062","mode":"CW"},
        {"activator":"G0CCC","reference":"GB-0222","frequency":"18130","mode":"SSB"},
        {"activator":"K4DDD","reference":"US-4567","frequency":"14074","mode":"FT8"},
        {"activator":"IZ1EEE","reference":"IT-0033","frequency":"7090","mode":"SSB"}])", hc::filterPota, hc::applyPota);
    feed(m, R"([{"activatorCallsign":"OE5ZZ/P","summitCode":"OE/TI-012","frequency":"10.118","mode":"cw"},
        {"activatorCallsign":"HB9BB/P","summitCode":"HB/BE-101","frequency":"7.032","mode":"cw"},
        {"activatorCallsign":"EA2FF/P","summitCode":"EA2/NV-004","frequency":"145.500","mode":"fm"}])", hc::filterSota, hc::applySota);
    feed(m, R"({"items":[{"title":"3Y0K Bouvet: team QRV until Nov 20 on 160-10 m"},{"title":"VP8/G4ABC Falkland Is. active from Nov 14"},
        {"title":"CQ WW CW contest Nov 28-29"}]})", hc::filterNews, hc::applyNews);
    // satelites de ejemplo con la epoca de ahora, por el mismo lector que en la placa
    {
        struct tm tm;
        gmtime_r(&now, &tm);
        char ep[40];
        strftime(ep, sizeof(ep), "%Y-%m-%dT%H:%M:%S", &tm);
        static const struct { const char* n; double mm, e, inc, raan, argp, ma; } sat[] = {
            {"ISS", 15.5, 0.0005, 51.64, 100, 50, 200}, {"SO-50", 14.76, 0.006, 64.55, 10, 20, 30}, {"RS-44", 12.8, 0.0218, 82.52, 250, 300, 120},
            {"AO-7", 12.54, 0.0012, 101.99, 330, 80, 280}, {"AO-91", 14.8, 0.025, 97.7, 60, 110, 300}, {"IO-117", 2.0, 0.0003, 70.0, 20, 0, 40},
            {"FO-29", 13.53, 0.035, 98.5, 200, 150, 10}};
        std::string j = "{\"timestamp\":1,\"data\":{";
        for (size_t k = 0; k < sizeof(sat) / sizeof(sat[0]); k++) {
            char b[400];
            snprintf(b, sizeof(b), "%s\"S%zu\":{\"name\":\"%s\",\"omm\":{\"EPOCH\":\"%s\",\"MEAN_MOTION\":%g,\"ECCENTRICITY\":%g,"
                     "\"INCLINATION\":%g,\"RA_OF_ASC_NODE\":%g,\"ARG_OF_PERICENTER\":%g,\"MEAN_ANOMALY\":%g}}", k ? "," : "", k, sat[k].n, ep,
                     sat[k].mm, sat[k].e, sat[k].inc, sat[k].raan, sat[k].argp, sat[k].ma);
            j += b;
        }
        feed(m, j + "}}", hc::filterSats, hc::applySats);
        double la, lo;
        hc::gridToLatLon(cfg.grid, la, lo);
        m.nPasses = hc::findPasses(m.sats, m.nSats, now, la, lo, m.passes, hc::MAX_PASSES);
        m.revPasses++;
    }
    m.wifi = true; m.rssi = -58; m.revLink++;
    m.rigLink = true; m.rigHz = 7155000; strcpy(m.rigMode, "LSB"); m.revRig++;

    hcui::begin(&m, &cfg, onSave, onTune);
    run(30);
    save((out + "/hc_1_start.raw").c_str());
    tap(1024 - 8 - 262 + 100, 60 + 45 + 14);                 // primer spot de la lista: pasa a ser el DX
    run(30);
    tap(8 + 216 - 42, 60 + 168 + 8 + 150 + 8 + 12);           // TUNE: tiene que salir la orden con la frecuencia y el modo del spot
    m.rigHz = 14074000; strcpy(m.rigMode, "PKTUSB"); m.revRig++;   // y la radio contesta en la lectura siguiente
    run(200);
    save((out + "/hc_2_dx.raw").c_str());
    tap(8 + 216 + 8 + 60, 60 + 258 + 8 + 31 + 23 + 8);        // segunda activacion (SOTA): pasa a ser el destino de TUNE
    tap(8 + 216 - 42, 60 + 168 + 8 + 150 + 8 + 12);
    run(200);
    save((out + "/hc_4_sota.raw").c_str());
    tap(1000, 26);                                           // rueda: ajustes
    run(60);
    save((out + "/hc_3_settings.raw").c_str());
    return 0;
}

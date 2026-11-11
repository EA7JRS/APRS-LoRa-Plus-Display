/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Cada dato tiene su ruta de la API y su periodo (mas largo que la cache del servidor, para no cargarlo). El JSON se lee
 * directamente de la conexion con un filtro de ArduinoJson y la memoria sale de la PSRAM. HTTP/1.0 para que el servidor no
 * conteste por trozos (chunked), que ArduinoJson no sabe leer de la conexion.
 * HTTPS va sin comprobar el certificado (setInsecure): son datos publicos de solo lectura y no se envia nada privado.
 */
#include "hc_net.h"
#include "hc_rig.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <cstdarg>

#ifndef DISPLAY_VERSION
#define DISPLAY_VERSION "dev"
#endif

namespace hcnet {

static hc::Model* M = nullptr;
static hc::Config cfg;
static SemaphoreHandle_t mtx = nullptr;

void lock() { if (mtx) xSemaphoreTake(mtx, portMAX_DELAY); }
void unlock() { if (mtx) xSemaphoreGive(mtx); }

struct PsramAllocator : ArduinoJson::Allocator {
    void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    void deallocate(void* p) override { heap_caps_free(p); }
    void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
};
static PsramAllocator psram;

static void setStatus(const char* fmt, ...) {
    char b[sizeof(M->status)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    lock();
    if (strcmp(M->status, b)) { strcpy(M->status, b); M->revLink++; }
    unlock();
}

struct Feed {
    const char* path;
    const char* name;
    void (*filter)(JsonDocument&);
    bool (*apply)(hc::Model&, JsonVariantConst);
    uint32_t periodMs;
    uint32_t next;                 // millis() de la proxima peticion (0 = ya)
    bool done;
};
static Feed feeds[] = {
    {"/api/dxcluster/paths", "spots", hc::filterSpots, hc::applySpots, 60000, 0, false},
    {"/api/solar-indices", "solar", hc::filterSolar, hc::applySolar, 10 * 60000, 0, false},
    {"/api/n0nbh", "bands", hc::filterN0nbh, hc::applyN0nbh, 30 * 60000, 0, false},
    {"/api/pota/spots", "POTA", hc::filterPota, hc::applyPota, 2 * 60000, 0, false},
    {"/api/sota/spots", "SOTA", hc::filterSota, hc::applySota, 3 * 60000, 0, false},
    {"/api/satellites/data", "sats", hc::filterSats, hc::applySats, 6 * 3600000u, 0, false},
    {"/api/dxnews", "news", hc::filterNews, hc::applyNews, 30 * 60000, 0, false},
};

static bool fetch(Feed& fd) {
    String url = String(cfg.server) + fd.path;
    WiFiClientSecure tls;
    WiFiClient plain;
    HTTPClient http;
    bool ok;
    if (url.startsWith("https://")) { tls.setInsecure(); ok = http.begin(tls, url); }
    else ok = http.begin(plain, url);
    if (!ok) { setStatus("bad server URL"); return false; }
    http.useHTTP10(true);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("APRS-LoRa-Plus-Display/" DISPLAY_VERSION " (hamclock; ESP32-S3)");
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[net] %s: HTTP %d\n", fd.path, code);
        if (code < 0) setStatus("%s: no answer", fd.name); else setStatus("%s: HTTP %d", fd.name, code);
        http.end();
        return false;
    }
    JsonDocument filter, doc(&psram);
    fd.filter(filter);
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
        Serial.printf("[net] %s: JSON %s\n", fd.path, err.c_str());
        setStatus("%s: bad data", fd.name);
        return false;
    }
    lock();
    ok = fd.apply(*M, doc.as<JsonVariantConst>());
    unlock();
    if (!ok) { Serial.printf("[net] %s: respuesta sin datos\n", fd.path); setStatus("%s: no data", fd.name); }
    return ok;
}

// pases: se calculan fuera del cerrojo (tardan) con una copia de los elementos
static void computePasses() {
    static hc::SatElements sats[hc::MAX_SATS];
    hc::Pass passes[hc::MAX_PASSES];
    double lat, lon;
    time_t now = time(nullptr);
    if (now < 1700000000 || !hc::gridToLatLon(cfg.grid, lat, lon)) return;
    lock();
    int n = M->nSats;
    memcpy(sats, M->sats, sizeof(sats[0]) * n);
    unlock();
    if (!n) return;
    uint32_t t0 = millis();
    int np = hc::findPasses(sats, n, now, lat, lon, passes, hc::MAX_PASSES);
    Serial.printf("[net] pases: %d satelites, %d pases, %u ms\n", n, np, (unsigned)(millis() - t0));
    lock();
    memcpy(M->passes, passes, sizeof(passes[0]) * np);
    M->nPasses = np;
    M->revPasses++;
    unlock();
}

static void task(void*) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setHostname("aprs-lora-hamclock");
    if (cfg.ssid[0]) WiFi.begin(cfg.ssid, cfg.pass);
    // hora: NTP como el resto del ecosistema (ntp.roa.es, hora oficial de Espana; de reserva hora.roa.es y pool.ntp.org)
    configTzTime(cfg.tz, "ntp.roa.es", "hora.roa.es", "pool.ntp.org");
    uint32_t tLink = 0, tPasses = 0;
    bool passesDone = false;
    for (;;) {
        uint32_t now = millis();
        bool up = WiFi.status() == WL_CONNECTED;
        if (now - tLink > 5000 || !tLink) {
            tLink = now ? now : 1;
            int rssi = up ? WiFi.RSSI() : 0;
            lock();
            if (M->wifi != up || abs(M->rssi - rssi) >= 3) { M->wifi = up; M->rssi = rssi; M->revLink++; }
            unlock();
        }
        if (!up) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        bool allOk = true, any = false;
        for (Feed& fd : feeds) {
            if (fd.next && (int32_t)(millis() - fd.next) < 0) continue;
            any = true;
            bool ok = fetch(fd);
            // si falla se reintenta al minuto (lo que ya habia en pantalla se queda)
            fd.next = millis() + (ok ? fd.periodMs : 60000);
            if (!fd.next) fd.next = 1;
            fd.done = fd.done || ok;
            allOk = allOk && ok;
            if (ok && fd.apply == hc::applySats) passesDone = false;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (any && allOk) setStatus("");
        if (!passesDone || millis() - tPasses > 10 * 60000) {
            if (time(nullptr) > 1700000000) { computePasses(); passesDone = true; tPasses = millis(); }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------- CAT: una conexion TCP con rigctld; lee frecuencia y modo cada 1,5 s y sintoniza cuando se le pide ----------
static bool tunePending = false;
static double tuneMhz = 0;
static char tuneMode[8] = "";

void tune(double mhz, const char* spotMode) {
    lock();
    tuneMhz = mhz;
    snprintf(tuneMode, sizeof(tuneMode), "%s", spotMode ? spotMode : "");
    tunePending = true;
    unlock();
}
static bool rigSend(void* ctx, const char* t) {
    WiFiClient* c = (WiFiClient*)ctx;
    return c->connected() && c->print(t) == strlen(t);
}
static bool rigLine(void* ctx, char* buf, size_t n, int timeoutMs) {
    WiFiClient* c = (WiFiClient*)ctx;
    size_t len = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < (uint32_t)timeoutMs) {
        int ch = c->read();
        if (ch < 0) {
            if (!c->connected()) return false;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (ch == '\n') { buf[len] = 0; return true; }
        if (ch != '\r' && len < n - 1) buf[len++] = (char)ch;
    }
    return false;
}
static void setRig(bool link, double hz, const char* mode) {
    lock();
    if (M->rigLink != link || M->rigHz != hz || strcmp(M->rigMode, mode)) {
        M->rigLink = link;
        M->rigHz = hz;
        snprintf(M->rigMode, sizeof(M->rigMode), "%s", mode);
        M->revRig++;
    }
    unlock();
}
static void rigTask(void*) {
    char host[64];
    int port;
    if (!hcrig::parseAddress(cfg.rig, host, sizeof(host), port)) { vTaskDelete(nullptr); return; }
    WiFiClient client;
    hcrig::Io io{rigSend, rigLine, &client};
    double hz = 0;
    char mode[8] = "";
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        if (!client.connected()) {
            client.stop();
            setRig(false, hz, mode);
            if (!client.connect(host, (uint16_t)port, 3000)) { vTaskDelay(pdMS_TO_TICKS(5000)); continue; }
            client.setNoDelay(true);
            Serial.printf("[rig] conectado a rigctld %s:%d\n", host, port);
        }
        lock();
        bool doTune = tunePending;
        double mhz = tuneMhz;
        char tm[8];
        strcpy(tm, tuneMode);
        tunePending = false;
        unlock();
        if (doTune) {
            bool ok = hcrig::tune(io, mhz, tm);
            Serial.printf("[rig] %.3f MHz %s: %s\n", mhz, tm, ok ? "ok" : "la radio no lo acepta");
            if (!ok) setStatus("rig: tune failed");
        }
        if (hcrig::getFreq(io, hz) && hcrig::getMode(io, mode)) setRig(true, hz, mode);
        else { Serial.println("[rig] sin respuesta: se cierra la conexion"); client.stop(); }
        // espera 1,5 s, pero una orden de sintonizar sale enseguida
        for (int i = 0; i < 15 && !tunePending; i++) vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void begin(hc::Model* model, const hc::Config* c) {
    M = model;
    cfg = *c;
    mtx = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(task, "hcnet", 12 * 1024, nullptr, 1, nullptr, 0);
    if (cfg.rig[0]) xTaskCreatePinnedToCore(rigTask, "hcrig", 6 * 1024, nullptr, 1, nullptr, 0);
}

}

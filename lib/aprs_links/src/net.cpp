/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "net.h"

#ifdef ESP32
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "gnss.h"
#include "signalk.h"
#endif

namespace net {

static Status st;
const Status& status() { return st; }

#ifdef ESP32

static Config cfg;
static volatile uint32_t cfgVersion = 0;
static SemaphoreHandle_t cfgLock = nullptr;
static StreamBufferHandle_t sbTcp = nullptr, sbUdp = nullptr, sbOut = nullptr;
static double posLat = 0, posLon = 0;
static bool posValid = false;
static Weather wx;
static bool wxNew = false;

// El tiempo en la posicion del barco. Bloquea unos segundos como mucho: va en la tarea de red, no en la de la pantalla.
static bool fetchWeather(const char* key, double lat, double lon) {
    WiFiClientSecure tls;
    tls.setInsecure();                                             // cifrado, sin comprobar el certificado (no hay reloj fiable al arrancar)
    HTTPClient http;
    char url[200];
    snprintf(url, sizeof(url), "https://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&units=metric&appid=%s", lat, lon, key);
    http.setConnectTimeout(6000);
    http.setTimeout(6000);
    if (!http.begin(tls, url)) return false;
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return false;
    Weather w;
    w.windMs = doc["wind"]["speed"] | NAN;
    w.windDeg = doc["wind"]["deg"] | NAN;
    w.gustMs = doc["wind"]["gust"] | NAN;
    w.pressureHpa = doc["main"]["pressure"] | NAN;
    w.tempC = doc["main"]["temp"] | NAN;
    w.humidityPct = doc["main"]["humidity"] | NAN;
    w.visibilityM = doc["visibility"] | NAN;
    snprintf(w.place, sizeof(w.place), "%s", (const char*)(doc["name"] | ""));
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    wx = w;
    wxNew = true;
    xSemaphoreGive(cfgLock);
    return true;
}

static void task(void*) {
    Config c;
    uint32_t seen = 0xFFFFFFFFu, lastBegin = 0, lastTry = 0, lastInfo = 0, nextWx = 0;
    bool wifiStarted = false, udpOn = false;
    WiFiClient client;
    WiFiUDP udp;
    static uint8_t buf[1024];
    for (;;) {
        uint32_t now = millis();
        if (seen != cfgVersion) {                                  // configuracion nueva: se empieza de cero
            xSemaphoreTake(cfgLock, portMAX_DELAY);
            c = cfg;
            seen = cfgVersion;
            xSemaphoreGive(cfgLock);
            client.stop();
            if (udpOn) { udp.stop(); udpOn = false; }
            if (wifiStarted) WiFi.disconnect(true);
            wifiStarted = false;
            st.wifi = st.tcp = false;
            st.ip[0] = 0;
            lastBegin = lastTry = 0;
            nextWx = now;
        }
        if (!c.ssid[0]) { vTaskDelay(pdMS_TO_TICKS(300)); continue; }
        if (WiFi.status() != WL_CONNECTED) {
            if (st.wifi) { st.wifi = st.tcp = false; st.ip[0] = 0; client.stop(); if (udpOn) { udp.stop(); udpOn = false; } }
            if (!wifiStarted || now - lastBegin > 20000) {         // reintento cada 20 s
                WiFi.mode(WIFI_STA);
                WiFi.setAutoReconnect(true);
                WiFi.begin(c.ssid, c.pass);
                wifiStarted = true;
                lastBegin = now;
            }
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        if (!st.wifi || now - lastInfo > 5000) {
            lastInfo = now;
            snprintf(st.ip, sizeof(st.ip), "%s", WiFi.localIP().toString().c_str());
            st.rssi = WiFi.RSSI();
            st.wifi = true;
        }
        // Tiempo de internet: cada 15 min (a los 2 min si ha fallado), con clave y posicion
        if (c.owmKey[0] && (int32_t)(now - nextWx) >= 0) {
            double la, lo;
            xSemaphoreTake(cfgLock, portMAX_DELAY);
            bool ok = posValid;
            la = posLat; lo = posLon;
            xSemaphoreGive(cfgLock);
            if (!ok) nextWx = now + 5000;                           // aun sin posicion
            else if (fetchWeather(c.owmKey, la, lo)) { st.wxOk++; nextWx = now + 15 * 60000u; }
            else { st.wxFail++; nextWx = now + 2 * 60000u; }
        }
        // UDP: lo que otros equipos manden a este puerto (AIS de AIS-catcher o AIS Dispatcher, NMEA de un multiplexor)
        if (c.udpPort > 0 && !udpOn) udpOn = udp.begin((uint16_t)c.udpPort) == 1;
        if (udpOn) {
            int n;
            while ((n = udp.parsePacket()) > 0) {
                int got = udp.read(buf, sizeof(buf) - 1);
                if (got <= 0) break;
                if (buf[got - 1] != '\n') buf[got++] = '\n';
                xStreamBufferSend(sbUdp, buf, (size_t)got, 0);     // si el bucle principal no da abasto, se pierde
                st.udpBytes += (uint32_t)got;
            }
        }
        // TCP: un servidor de la red local
        if (c.tcpKind != TCP_OFF && c.tcpPort > 0) {
            if (!client.connected()) {
                st.tcp = false;
                if (lastTry == 0 || now - lastTry > 10000) {       // reintento cada 10 s
                    lastTry = now ? now : 1;
                    // Sin servidor escrito: el equipo que da la WiFi (el telefono con la app, que sirve NMEA en el 10110)
                    bool ok = c.host[0] ? client.connect(c.host, (uint16_t)c.tcpPort, 3000) : client.connect(WiFi.gatewayIP(), (uint16_t)c.tcpPort, 3000);
                    if (ok) {
                        client.setNoDelay(true);
                        if (c.tcpKind == TCP_GPSD) client.print(plotter::kGpsdWatchNmea);   // gpsd: que mande el NMEA tal cual
                        if (c.tcpKind == TCP_SIGNALK) {                 // Signal K: todo lo del barco propio, una vez por segundo
                            char sub[120];
                            int n = plotter::skSubscribe(sub, sizeof(sub), 1000);
                            if (n) client.write((const uint8_t*)sub, (size_t)n);
                        }
                        st.tcp = true;
                    }
                }
            } else {
                st.tcp = true;
                size_t out = xStreamBufferReceive(sbOut, buf, sizeof(buf), 0);       // lo que haya para enviar
                if (out) client.write(buf, out);
                int avail = client.available();
                while (avail > 0) {
                    int got = client.read(buf, avail > (int)sizeof(buf) ? sizeof(buf) : (size_t)avail);
                    if (got <= 0) break;
                    xStreamBufferSend(sbTcp, buf, (size_t)got, 0);
                    st.tcpBytes += (uint32_t)got;
                    avail = client.available();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void begin() {
    if (cfgLock) return;
    cfgLock = xSemaphoreCreateMutex();
    sbTcp = xStreamBufferCreate(8192, 1);
    sbUdp = xStreamBufferCreate(4096, 1);
    sbOut = xStreamBufferCreate(1024, 1);
    xTaskCreatePinnedToCore(task, "net", 12288, nullptr, 1, nullptr, 0);    // la pila, holgada por el TLS
}

void apply(const Config& c) {
    if (!cfgLock) begin();
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    cfg = c;
    cfgVersion = cfgVersion + 1;
    xSemaphoreGive(cfgLock);
}

void setPosition(double lat, double lon) {
    if (!cfgLock) return;
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    posLat = lat; posLon = lon; posValid = true;
    xSemaphoreGive(cfgLock);
}

bool takeWeather(Weather& w) {
    if (!cfgLock || !wxNew) return false;
    xSemaphoreTake(cfgLock, portMAX_DELAY);
    w = wx;
    wxNew = false;
    xSemaphoreGive(cfgLock);
    return true;
}

int readTcp(uint8_t* b, int max) { return sbTcp ? (int)xStreamBufferReceive(sbTcp, b, (size_t)max, 0) : 0; }
void sendTcp(const char* d, int len) { if (sbOut && st.tcp && len > 0) xStreamBufferSend(sbOut, d, (size_t)len, 0); }
int readUdp(uint8_t* b, int max) { return sbUdp ? (int)xStreamBufferReceive(sbUdp, b, (size_t)max, 0) : 0; }

#else   // en el PC no hay red

void begin() {}
void apply(const Config&) {}
int readTcp(uint8_t*, int) { return 0; }
void sendTcp(const char*, int) {}
void setPosition(double, double) {}
bool takeWeather(Weather&) { return false; }
int readUdp(uint8_t*, int) { return 0; }

#endif

}

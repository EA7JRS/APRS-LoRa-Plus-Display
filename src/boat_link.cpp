/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "boat_link.h"

#include <Arduino.h>
#include <Preferences.h>

#include <cmath>
#include <cstdio>

#include "nmea.h"
#include "sk_link.h"
#include "stream.h"

using namespace plotter;

namespace boat {

static DataStore st_;
static AisTargets tgs;
static Stats st;
static net::Config cfg;
static Nmea0183 tcpNmea, udpNmea;
static LineBuffer tcpLine, udpLine;

DataStore& store() { return st_; }
AisTargets& targets() { return tgs; }
const Stats& stats() { return st; }
const net::Config& config() { return cfg; }

const char* sourceName(Source s) {
    switch (s) {
        case Source::Sensor: return "SENS";
        case Source::Can: return "N2K";
        case Source::Uart: return "SER";
        case Source::Tcp: return "LAN";
        case Source::Ble: return "TRK";
        case Source::Internet: return "WEB";
        default: return "-";
    }
}
const char* kindName(int k) {
    switch (k) {
        case net::TCP_NMEA: return "NMEA 0183";
        case net::TCP_GPSD: return "gpsd";
        case net::TCP_SIGNALK: return "Signal K";
        default: return "Off";
    }
}
int defaultPort(int k) { return k == net::TCP_GPSD ? 2947 : k == net::TCP_SIGNALK ? 8375 : 10110; }

// Misma coleccion "display" de Preferences que la paleta
static void load() {
    Preferences p;
    p.begin("display", true);
    snprintf(cfg.ssid, sizeof cfg.ssid, "%s", p.getString("wifiSsid", "").c_str());
    snprintf(cfg.pass, sizeof cfg.pass, "%s", p.getString("wifiPass", "").c_str());
    snprintf(cfg.host, sizeof cfg.host, "%s", p.getString("tcpHost", "").c_str());
    cfg.tcpKind = p.getInt("tcpKind", net::TCP_OFF);
    if (cfg.tcpKind < 0 || cfg.tcpKind >= net::TCP_KINDS) cfg.tcpKind = net::TCP_OFF;
    cfg.tcpPort = p.getInt("tcpPort", 10110);
    cfg.udpPort = p.getInt("udpPort", 0);
    snprintf(cfg.owmKey, sizeof cfg.owmKey, "%s", p.getString("owmKey", "").c_str());
    p.end();
}

void setConfig(const net::Config& c) {
    cfg = c;
    Preferences p;
    p.begin("display", false);
    p.putString("wifiSsid", cfg.ssid);
    p.putString("wifiPass", cfg.pass);
    p.putString("tcpHost", cfg.host);
    p.putInt("tcpKind", cfg.tcpKind);
    p.putInt("tcpPort", cfg.tcpPort);
    p.putInt("udpPort", cfg.udpPort);
    p.putString("owmKey", cfg.owmKey);
    p.end();
    net::apply(cfg);
    Serial.printf("[boat] red: WiFi \"%s\", servidor %s %s:%d, UDP %d\n", cfg.ssid, kindName(cfg.tcpKind), cfg.host[0] ? cfg.host : "(puerta de enlace)",
                  cfg.tcpPort, cfg.udpPort);
}

void begin() {
    load();
    net::begin();
    net::apply(cfg);
}

void trackerFix(double lat, double lon, double sogKn, double cogDeg, uint32_t now) {
    st_.set(Key::Lat, lat, Source::Ble, now);
    st_.set(Key::Lon, lon, Source::Ble, now);
    st_.set(Key::Sog, sogKn, Source::Ble, now);
    if (sogKn > 0.5) st_.set(Key::Cog, cogDeg, Source::Ble, now);   // parado, el rumbo del GNSS no vale
}

// Signal K: cada linea es un JSON, mucho mas larga que una sentencia NMEA
static void signalK(uint32_t now) {
    static uint8_t buf[512];
    static char sk[4096];
    static int len = 0;
    static bool over = false;
    for (int round = 0; round < 8; round++) {
        int n = net::readTcp(buf, sizeof buf);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\n') {
                if (!over && len > 0) {
                    sk[len] = 0;
                    sk::Notice nt;
                    st.skValues += (uint32_t)sk::feedLine(sk, now, st_, nt);
                    st.tcpLines++;
                    st.netLastMs = now;
                    if (nt.present) {
                        snprintf(st.skKey, sizeof st.skKey, "%s", nt.key);
                        snprintf(st.skState, sizeof st.skState, "%s", nt.state);
                        snprintf(st.skMessage, sizeof st.skMessage, "%s", nt.message);
                        st.skSeq++;
                    }
                }
                len = 0;
                over = false;
            } else if (c != '\r') {
                if (len < (int)sizeof sk - 1) sk[len++] = c;
                else over = true;                                   // linea demasiado larga: se descarta entera
            }
        }
    }
}

static void nmeaLines(uint32_t now) {
    static uint8_t buf[512];
    for (int pass = cfg.tcpKind == net::TCP_SIGNALK ? 1 : 0; pass < 2; pass++) {
        LineBuffer& lb = pass ? udpLine : tcpLine;
        Nmea0183& rd = pass ? udpNmea : tcpNmea;
        uint32_t& lines = pass ? st.udpLines : st.tcpLines;
        for (int round = 0; round < 4; round++) {
            int n = pass ? net::readUdp(buf, sizeof buf) : net::readTcp(buf, sizeof buf);
            if (n <= 0) break;
            for (int i = 0; i < n; i++) {
                if (!lb.push((char)buf[i])) continue;
                lines++;
                st.netLastMs = now;
                if (rd.feed(lb.line(), Source::Tcp, now, st_) == NmeaResult::Ais) {
                    tgs.update(rd.ais(), now);
                    st.aisMessages++;
                }
            }
        }
    }
}

// OpenWeatherMap: viento, presion, temperatura... como fuente "internet" y calidad "estimado"
static void weather(uint32_t now) {
    net::Weather w;
    if (!net::takeWeather(w)) return;
    const double kn = 3600.0 / 1852.0;
    auto put = [&](Key k, double v) { if (!std::isnan(v)) st_.set(k, v, Source::Internet, now, Quality::Estimated); };
    put(Key::Tws, w.windMs * kn);
    put(Key::Twd, w.windDeg);
    put(Key::Gust, w.gustMs * kn);
    put(Key::Pressure, w.pressureHpa);
    put(Key::AirTemp, w.tempC);
    put(Key::Humidity, w.humidityPct);
    put(Key::Visibility, w.visibilityM / 1852.0);
    snprintf(st.wxPlace, sizeof st.wxPlace, "%s", w.place);
    st.wxMs = now ? now : 1;
}

void loop(uint32_t now) {
    if (cfg.tcpKind == net::TCP_SIGNALK) signalK(now);
    nmeaLines(now);
    weather(now);
    static uint32_t last = 0;
    if (now - last >= 1000) {                                       // cada segundo: posicion para el tiempo, distancias y CPA
        last = now;
        double la = 0, lo = 0, sog = 0, cog = 0;
        const bool own = st_.get(Key::Lat, now, la) && st_.get(Key::Lon, now, lo);
        if (own) net::setPosition(la, lo);
        st_.get(Key::Sog, now, sog);
        st_.get(Key::Cog, now, cog);
        tgs.compute(own, Mover{{own ? la : 0.0, own ? lo : 0.0}, sog, cog}, now);
    }
}
}

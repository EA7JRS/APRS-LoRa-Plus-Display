/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "sim.h"

#include <cmath>
#include <cstdio>

#include "motion.h"
#include "nmea.h"

namespace plotter {

NmeaSimulator::NmeaSimulator(LatLon start, double cog, double sog) : pos_(start), cog_(cog), sog_(sog), heading_(cog) {
    // Nombres y MMSI inventados (224 = Espana). El primero viene de vuelta encontrada: salta la alarma a los pocos minutos.
    tg_[0] = {224900110, "SIMULADO UNO", gcDestination(start, cog + 4, 2.6), norm360(cog + 182), 9.0};
    tg_[1] = {224900220, "SIMULADO DOS", gcDestination(start, cog - 70, 1.8), norm360(cog + 95), 5.1};
    tg_[2] = {224900330, "PESQUERO TRES", gcDestination(start, cog + 120, 2.2), norm360(cog - 30), 3.4};
    tg_[3] = {224900440, "FONDEADO CUATRO", gcDestination(start, cog + 60, 1.1), 0, 0.0};
    tg_[4] = {224900550, "CARGUERO CINCO", gcDestination(start, cog - 150, 4.5), norm360(cog + 20), 14.0};
}

void NmeaSimulator::step(uint32_t now) {
    if (!started_) { started_ = true; last_ = t0_ = now; return; }
    double dt = uint32_t(now - last_) / 1000.0;
    last_ = now;
    if (dt <= 0 || dt > 60) return;
    double t = uint32_t(now - t0_) / 1000.0;
    // Pequenas variaciones lentas para que los numeros se muevan como en la mar
    double cog = cog_ + 4.0 * std::sin(t / 37.0), sog = sog_ + 0.4 * std::sin(t / 23.0);
    heading_ = norm360(cog + 3.0 * std::sin(t / 11.0));
    depth_ = 12.4 + 2.5 * std::sin(t / 61.0);
    tws_ = 11.0 + 3.0 * std::sin(t / 47.0);
    twd_ = norm360(285.0 + 12.0 * std::sin(t / 83.0));
    pos_ = gcDestination(pos_, cog, sog * dt / 3600.0);
    for (Target& g : tg_)
        if (g.sog > 0) g.pos = gcDestination(g.pos, g.cog, g.sog * dt / 3600.0);
}

int NmeaSimulator::aisSentence(int i, char* out, int size) const {
    if (i < 0 || i >= aisCount()) return 0;
    const Target& g = tg_[i / 2];
    if (i % 2) return aisEncodeName(out, size, g.mmsi, g.name);
    return aisEncodePosition(out, size, g.mmsi, g.pos.lat, g.pos.lon, g.sog, g.cog, g.sog > 0 ? int(g.cog) % 360 : 511, g.sog > 0 ? 0 : 1);
}

static int finish(char* out, int size, const char* body) {
    int n = std::snprintf(out, size_t(size), "$%s*%02X", body, nmeaChecksum(body));
    return (n > 0 && n < size) ? n : 0;
}

static void latLonFields(LatLon p, char* out, int size) {
    double la = std::fabs(p.lat), lo = std::fabs(p.lon);
    int lad = int(la), lod = int(lo);
    std::snprintf(out, size_t(size), "%02d%07.4f,%c,%03d%07.4f,%c", lad, (la - lad) * 60.0, p.lat < 0 ? 'S' : 'N', lod, (lo - lod) * 60.0,
                  p.lon < 0 ? 'W' : 'E');
}

int NmeaSimulator::sentence(int i, char* out, int size) const {
    char b[96], ll[40];
    double t = uint32_t(last_ - t0_) / 1000.0;
    double cog = norm360(cog_ + 4.0 * std::sin(t / 37.0)), sog = sog_ + 0.4 * std::sin(t / 23.0);
    uint32_t s = uint32_t(t) + 11 * 3600 + 25 * 60;                   // la hora del simulador empieza a las 11:25:00
    latLonFields(pos_, ll, sizeof(ll));
    switch (i) {
        case 0:
            std::snprintf(b, sizeof(b), "GPRMC,%02u%02u%02u,A,%s,%.1f,%.1f,111126,,,A", unsigned(s / 3600 % 24), unsigned(s / 60 % 60),
                          unsigned(s % 60), ll, sog, cog);
            break;
        case 1:
            std::snprintf(b, sizeof(b), "GPGGA,%02u%02u%02u,%s,1,09,0.9,3.0,M,46.9,M,,", unsigned(s / 3600 % 24), unsigned(s / 60 % 60),
                          unsigned(s % 60), ll);
            break;
        case 2: std::snprintf(b, sizeof(b), "HCHDT,%.1f,T", heading_); break;
        case 3: std::snprintf(b, sizeof(b), "SDDPT,%.1f,0.0", depth_); break;
        case 4: {
            double awa, aws;
            apparentWind(twd_, tws_, sog, heading_, awa, aws);
            std::snprintf(b, sizeof(b), "WIMWV,%.1f,R,%.1f,N,A", norm360(awa), aws);
            break;
        }
        case 5: std::snprintf(b, sizeof(b), "VWVHW,%.1f,T,,M,%.1f,N,,K", heading_, sog - 0.3); break;
        case 6: std::snprintf(b, sizeof(b), "YXMTW,%.1f,C", waterC_); break;
        default: return 0;
    }
    return finish(out, size, b);
}

}  // namespace plotter

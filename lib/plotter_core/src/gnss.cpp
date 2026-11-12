/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gnss.h"

#include <cstdlib>
#include <cstring>

namespace plotter {

GnssSystem gnssSystemFromTalker(const char* t) {
    if (!t || !t[0] || !t[1]) return GnssSystem::Other;
    if (t[0] == 'G') {
        switch (t[1]) {
            case 'P': return GnssSystem::Gps;
            case 'L': return GnssSystem::Glonass;
            case 'A': return GnssSystem::Galileo;
            case 'B': return GnssSystem::Beidou;
            case 'Q': return GnssSystem::Qzss;
            case 'I': return GnssSystem::Navic;
            default: return GnssSystem::Other;
        }
    }
    if (t[0] == 'B' && t[1] == 'D') return GnssSystem::Beidou;
    if (t[0] == 'P' && t[1] == 'Q') return GnssSystem::Beidou;
    if (t[0] == 'Q' && t[1] == 'Z') return GnssSystem::Qzss;
    return GnssSystem::Other;
}

const char* gnssSystemName(GnssSystem s) {
    static const char* const k[] = {"GPS", "GLONASS", "Galileo", "BeiDou", "QZSS", "NavIC", "SBAS", "GNSS"};
    return k[int(s) <= 7 ? int(s) : 7];
}

// Con emisor GP los receptores mezclan SBAS (33-64) y a veces GLONASS (65-96) en la numeracion NMEA
static GnssSystem refine(GnssSystem sys, int prn) {
    if (sys == GnssSystem::Gps || sys == GnssSystem::Other) {
        if (prn >= 33 && prn <= 64) return GnssSystem::Sbas;
        if (prn >= 65 && prn <= 96) return GnssSystem::Glonass;
        if (prn >= 1 && prn <= 32) return GnssSystem::Gps;
    }
    return sys;
}

static GnssSystem fromSystemId(int id) {            // NMEA 4.10
    switch (id) {
        case 1: return GnssSystem::Gps;
        case 2: return GnssSystem::Glonass;
        case 3: return GnssSystem::Galileo;
        case 4: return GnssSystem::Beidou;
        case 5: return GnssSystem::Qzss;
        case 6: return GnssSystem::Navic;
        default: return GnssSystem::Other;
    }
}

void GnssSky::dropSystem(GnssSystem sys, uint8_t sig) {
    int w = 0;
    for (int i = 0; i < n_; i++) {
        bool mine = sig_[i] == sig && org_[i] == sys;         // por emisor: con GP llegan tambien SBAS y a veces GLONASS
        if (!mine) { s_[w] = s_[i]; sig_[w] = sig_[i]; org_[w] = org_[i]; w++; }
    }
    n_ = w;
}

void GnssSky::feedGsv(const char* const* f, int n, uint32_t now) {
    if (n < 4 || std::strlen(f[0]) < 5) return;
    GnssSystem sys = gnssSystemFromTalker(f[0]);
    int num = std::atoi(f[2]);
    int groups = (n - 4) / 4;
    bool hasSig = (n - 4) % 4 == 1;                 // campo final con la senal (NMEA 4.10)
    uint8_t sig = hasSig ? uint8_t(std::strtol(f[n - 1], nullptr, 16)) : 0;
    if (num <= 1) {                                 // primera trama del grupo: lista nueva de esa constelacion y senal
        dropSystem(sys, sig);
        for (Group& g : grp_)
            if (g.on && g.sys == sys && g.sig == sig) g.on = false;
        for (Group& g : grp_)
            if (!g.on) { g = {sys, sig, now, true}; break; }
    } else {
        for (Group& g : grp_)
            if (g.on && g.sys == sys && g.sig == sig) g.t = now;
    }
    for (int k = 0; k < groups; k++) {
        const char* const* q = f + 4 + k * 4;
        if (!q[0][0]) continue;
        int prn = std::atoi(q[0]);
        if (prn <= 0 || n_ >= kMax) continue;
        GnssSat s;
        s.sys = refine(sys, prn);
        s.prn = uint16_t(prn);
        s.elev = q[1][0] ? int8_t(std::atoi(q[1])) : int8_t(-1);
        s.az = q[2][0] ? int16_t(std::atoi(q[2])) : int16_t(-1);
        s.snr = q[3][0] ? int8_t(std::atoi(q[3])) : int8_t(-1);
        for (int u = 0; u < nUsed_; u++)
            if (used_[u].prn == s.prn && (used_[u].anySys || used_[u].sys == s.sys)) s.used = true;
        s_[n_] = s;
        sig_[n_] = sig;
        org_[n_] = sys;
        n_++;
    }
}

void GnssSky::feedGsa(const char* const* f, int n, uint32_t now) {
    if (n < 18) return;
    if (uint32_t(now - usedT_) > 700 || nUsed_ == 0) nUsed_ = 0;       // rafaga nueva (una GSA por constelacion, seguidas)
    usedT_ = now;
    fix_ = std::atoi(f[2]);
    GnssSystem sys = gnssSystemFromTalker(f[0]);
    bool anySys = sys == GnssSystem::Other;                             // GN sin identificador de sistema
    if (n >= 19 && f[18][0]) { sys = fromSystemId(std::atoi(f[18])); anySys = sys == GnssSystem::Other; }
    for (int i = 3; i <= 14; i++) {
        if (!f[i][0]) continue;
        int prn = std::atoi(f[i]);
        if (prn <= 0 || nUsed_ >= kUsedMax) continue;
        used_[nUsed_++] = {anySys ? GnssSystem::Other : refine(sys, prn), uint16_t(prn), anySys};
    }
    if (f[15][0]) pdop_ = std::atof(f[15]);
    if (f[16][0]) hdop_ = std::atof(f[16]);
    if (f[17][0]) vdop_ = std::atof(f[17]);
    for (int i = 0; i < n_; i++) {
        s_[i].used = false;
        for (int u = 0; u < nUsed_; u++)
            if (used_[u].prn == s_[i].prn && (used_[u].anySys || used_[u].sys == s_[i].sys)) s_[i].used = true;
    }
}

int GnssSky::usedCount() const {
    int c = 0;
    for (int i = 0; i < n_; i++) {
        if (!s_[i].used) continue;
        bool dup = false;                           // el mismo satelite en dos senales cuenta una vez
        for (int j = 0; j < i; j++)
            if (s_[j].used && s_[j].prn == s_[i].prn && s_[j].sys == s_[i].sys) dup = true;
        if (!dup) c++;
    }
    return c;
}

void GnssSky::expire(uint32_t now, uint32_t maxAge) {
    for (Group& g : grp_)
        if (g.on && uint32_t(now - g.t) > maxAge) { dropSystem(g.sys, g.sig); g.on = false; }
    if (nUsed_ && uint32_t(now - usedT_) > maxAge) {
        nUsed_ = 0;
        fix_ = 0;
        for (int i = 0; i < n_; i++) s_[i].used = false;
    }
}

}  // namespace plotter

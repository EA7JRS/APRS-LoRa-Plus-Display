/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "n2ktx.h"

#include <cmath>
#include <cstring>

#include "n2k.h"

namespace plotter {

static constexpr double kMsPerKn = 1852.0 / 3600.0;
static constexpr uint32_t kIsoRequest = 59904, kAddressClaim = 60928;

uint32_t n2kMakeId(uint8_t prio, uint32_t pgn, uint8_t src, uint8_t dst) {
    uint32_t pf = (pgn >> 8) & 0xFF;
    uint32_t p = pf < 240 ? ((pgn & 0x3FF00) | dst) : (pgn & 0x3FFFF);
    return ((uint32_t)(prio & 7) << 26) | (p << 8) | src;
}

void n2kWind(uint8_t sid, double knots, double angleDeg, uint8_t ref, uint8_t out[8]) {
    std::memset(out, 0xFF, 8);
    out[0] = sid;
    if (std::isfinite(knots) && knots >= 0) {
        double v = std::round(knots * kMsPerKn * 100.0);
        if (v > 65532) v = 65532;                    // 0xFFFD..0xFFFF quedan para "sin dato" y similares
        uint16_t s = (uint16_t)v;
        out[1] = s & 0xFF; out[2] = s >> 8;
    }
    if (std::isfinite(angleDeg)) {
        double a = std::fmod(angleDeg, 360.0);
        if (a < 0) a += 360.0;
        double r = std::round(a * M_PI / 180.0 * 10000.0);
        if (r >= 62832) r = 0;                       // 2 pi redondeado = 0
        uint16_t u = (uint16_t)r;
        out[3] = u & 0xFF; out[4] = u >> 8;
    }
    out[5] = 0xF8 | (ref & 7);                       // los 5 bits altos, reservados, a 1
}

uint64_t n2kNameValue(const N2kName& n) {
    uint64_t v = 0;
    v |= (uint64_t)(n.unique & 0x1FFFFF);
    v |= (uint64_t)(n.manufacturer & 0x7FF) << 21;
    v |= (uint64_t)(n.deviceInstance) << 32;
    v |= (uint64_t)(n.deviceFunction) << 40;
    v |= (uint64_t)(n.deviceClass & 0x7F) << 49;      // el bit 48 queda reservado a 0
    v |= (uint64_t)(n.systemInstance & 0x0F) << 56;
    v |= (uint64_t)(n.industryGroup & 0x07) << 60;
    v |= (uint64_t)(n.arbitraryAddress ? 1 : 0) << 63;
    return v;
}

N2kAddressClaim::N2kAddressClaim(uint64_t name, uint8_t preferred) : name_(name), addr_(preferred > 251 ? 0 : preferred) {}

void N2kAddressClaim::claimFrame(Frame& out) const {
    out.id = n2kMakeId(6, kAddressClaim, addr_, 255);
    for (int i = 0; i < 8; i++) out.d[i] = (uint8_t)(name_ >> (8 * i));
    out.len = 8;
}

void N2kAddressClaim::start(uint32_t nowMs, Frame& out) {
    started_ = true;
    claimedMs_ = nowMs;
    claimFrame(out);
}

bool N2kAddressClaim::pickNext() {
    if (!(name_ >> 63)) { addr_ = 254; return false; }       // sin direccion arbitraria: se queda sin direccion
    for (int k = 1; k <= 252; k++) {
        uint8_t a = (uint8_t)((addr_ + k) % 252);
        if (!(taken_[a >> 3] & (1 << (a & 7)))) { addr_ = a; return true; }
    }
    addr_ = 254;
    return false;
}

bool N2kAddressClaim::onFrame(uint32_t canId, const uint8_t* d, int len, uint32_t nowMs, Frame& out) {
    if (!started_) return false;
    N2kId id = n2kParseId(canId);
    if (id.pgn == kIsoRequest && len >= 3) {
        uint32_t req = d[0] | (d[1] << 8) | ((uint32_t)d[2] << 16);
        if (req != kAddressClaim || (id.dst != 255 && id.dst != addr_)) return false;
        claimFrame(out);                                      // tambien desde 254, como pide la norma
        return true;
    }
    if (id.pgn != kAddressClaim || len < 8) return false;
    uint64_t other = 0;
    for (int i = 0; i < 8; i++) other |= (uint64_t)d[i] << (8 * i);
    if (id.src <= 251) taken_[id.src >> 3] |= (uint8_t)(1 << (id.src & 7));
    if (id.src != addr_ || other == name_) return false;
    if (name_ < other) {                                      // gana el NAME mas bajo: se repite la propia
        claimFrame(out);
        claimedMs_ = nowMs;
        return true;
    }
    pickNext();                                               // pierde: otra direccion libre, o 254
    changes_++;
    claimedMs_ = nowMs;
    claimFrame(out);
    return true;
}

bool N2kAddressClaim::ready(uint32_t nowMs) const { return started_ && !lost() && nowMs - claimedMs_ >= 250; }

}  // namespace plotter

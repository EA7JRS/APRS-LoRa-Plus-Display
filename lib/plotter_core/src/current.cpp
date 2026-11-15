/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "current.h"

#include <cmath>

namespace plotter {

static const double kD2R = 3.14159265358979323846 / 180.0;
static double n360(double a) { a = std::fmod(a, 360.0); return a < 0 ? a + 360.0 : a; }
static Current fromNE(double n, double e) { Current c; c.driftKn = std::sqrt(n * n + e * e); c.setDeg = c.driftKn > 1e-9 ? n360(std::atan2(e, n) / kD2R) : 0; return c; }

Current currentFromVectors(double hdg, double stw, double lw, double cog, double sog) {
    const double rs = (hdg + lw) * kD2R, cg = cog * kD2R;
    return fromNE(sog * std::cos(cg) - stw * std::cos(rs), sog * std::sin(cg) - stw * std::sin(rs));
}

bool currentFromDr(LatLon start, double course, double speed, double lw, double hours, LatLon fix, LatLon& dr, Current& c) {
    if (hours <= 0) return false;
    dr = rhumbDestination(start, n360(course + lw), speed * hours);
    const double d = rhumbDistanceNm(dr, fix);
    c.driftKn = d / hours;
    c.setDeg = d > 1e-9 ? rhumbBearing(dr, fix) : 0;
    return true;
}

void groundTrack(double hdg, double stw, double lw, const Current& c, double& cog, double& sog) {
    const double rs = (hdg + lw) * kD2R, st = c.setDeg * kD2R;
    const Current g = fromNE(stw * std::cos(rs) + c.driftKn * std::cos(st), stw * std::sin(rs) + c.driftKn * std::sin(st));
    cog = g.setDeg;
    sog = g.driftKn;
}

bool courseToSteer(double cog, double stw, double lw, const Current& c, double& hdg, double& sog) {
    if (stw <= 0) return false;
    const double rel = (c.setDeg - cog) * kD2R;
    const double s = -c.driftKn * std::sin(rel) / stw;     // seno del angulo entre el rumbo de superficie y el efectivo
    if (std::fabs(s) > 1) return false;
    const double a = std::asin(s);
    sog = stw * std::cos(a) + c.driftKn * std::cos(rel);
    if (sog <= 0) return false;
    hdg = n360(cog + a / kD2R - lw);
    return true;
}

void CurrentAverager::add(const Current& c, uint32_t now) {
    const double n = c.driftKn * std::cos(c.setDeg * kD2R), e = c.driftKn * std::sin(c.setDeg * kD2R);
    if (n_ == 0 || now - last_ > 30000) { nn_ = n; e_ = e; first_ = now; n_ = 1; last_ = now; return; }
    const double dt = (now - last_) / 1000.0;
    const double k = 1.0 - std::exp(-dt / tau_);
    nn_ += (n - nn_) * k;
    e_ += (e - e_) * k;
    last_ = now;
    n_++;
}

bool CurrentAverager::get(Current& c, uint32_t now) const {
    if (n_ == 0 || now - last_ > 30000 || last_ - first_ < 20000) return false;
    c = fromNE(nn_, e_);
    return true;
}

}  // namespace plotter

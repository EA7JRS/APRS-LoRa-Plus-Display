/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "motion.h"

#include <cmath>

namespace plotter {

bool cpa(const Mover& own, const Mover& tg, double& cpaNm, double& tcpaMin) {
    // Plano local centrado en el barco propio (millas): x al este, y al norte
    double d = gcDistanceNm(own.pos, tg.pos), b = deg2rad(gcInitialBearing(own.pos, tg.pos));
    double rx = d * std::sin(b), ry = d * std::cos(b);
    double vx = tg.sogKn * std::sin(deg2rad(tg.cogDeg)) - own.sogKn * std::sin(deg2rad(own.cogDeg));
    double vy = tg.sogKn * std::cos(deg2rad(tg.cogDeg)) - own.sogKn * std::cos(deg2rad(own.cogDeg));
    double v2 = vx * vx + vy * vy;
    if (v2 < 1e-6) { cpaNm = d; tcpaMin = 0; return false; }
    double t = -(rx * vx + ry * vy) / v2;                               // horas
    if (t < 0) { cpaNm = d; tcpaMin = t * 60; return true; }
    cpaNm = std::hypot(rx + vx * t, ry + vy * t);
    tcpaMin = t * 60;
    return true;
}

void trueWind(double awa, double aws, double boat, double hdg, double& twa, double& tws, double& twd) {
    double x = aws * std::sin(deg2rad(awa)), y = aws * std::cos(deg2rad(awa)) - boat;
    tws = std::hypot(x, y);
    twa = tws < 1e-9 ? 0 : rad2deg(std::atan2(x, y));
    twd = norm360(hdg + twa);
}

void apparentWind(double twd, double tws, double boat, double hdg, double& awa, double& aws) {
    double twa = deg2rad(norm180(twd - hdg));
    double x = tws * std::sin(twa), y = tws * std::cos(twa) + boat;
    aws = std::hypot(x, y);
    awa = aws < 1e-9 ? 0 : rad2deg(std::atan2(x, y));
}

double vmgToWind(double boat, double twa) { return boat * std::cos(deg2rad(twa)); }
double vmgToWaypoint(double sog, double cog, double brg) { return sog * std::cos(deg2rad(cog - brg)); }

void current(double hdg, double stw, double cog, double sog, double& set, double& drift) {
    double x = sog * std::sin(deg2rad(cog)) - stw * std::sin(deg2rad(hdg));
    double y = sog * std::cos(deg2rad(cog)) - stw * std::cos(deg2rad(hdg));
    drift = std::hypot(x, y);
    set = drift < 1e-9 ? 0 : norm360(rad2deg(std::atan2(x, y)));
}

}  // namespace plotter

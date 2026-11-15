/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "bearingfix.h"

#include <cmath>

namespace plotter {

static const double kRad = 3.14159265358979323846 / 180.0;

bool fixFromBearings(LatLon m1, double b1, LatLon m2, double b2, double runCourse, double runNm, BearingFix& out) {
    // plano local en millas con origen en el punto 2: x al este, y al norte
    const double k = std::cos(m2.lat * kRad);
    auto toXY = [&](LatLon p, double& x, double& y) { x = (p.lon - m2.lon) * 60.0 * k; y = (p.lat - m2.lat) * 60.0; };
    auto toLL = [&](double x, double y) { return LatLon{m2.lat + y / 60.0, m2.lon + x / (60.0 * k)}; };
    double x1, y1;
    toXY(m1, x1, y1);
    const double rx = runNm * std::sin(runCourse * kRad), ry = runNm * std::cos(runCourse * kRad);
    const double u1x = std::sin(b1 * kRad), u1y = std::cos(b1 * kRad), u2x = std::sin(b2 * kRad), u2y = std::cos(b2 * kRad);
    // barco = punto - d * u. Primera linea trasladada: m1 + R - t u1; segunda: -s u2. t u1 - s u2 = m1 + R
    const double det = -u1x * u2y + u2x * u1y;
    double cut = std::fabs(std::fmod(b2 - b1, 180.0));
    if (cut > 90) cut = 180 - cut;
    out.cutDeg = cut;
    if (std::fabs(det) < 1e-9 || cut < 10) return false;
    const double bx = x1 + rx, by = y1 + ry;
    const double t = (bx * (-u2y) - (-u2x) * by) / det;
    const double s = (u1x * by - u1y * bx) / det;
    if (t <= 0 || s <= 0) return false;
    const double fx = -s * u2x, fy = -s * u2y;
    out.fix = toLL(fx, fy);
    out.first = toLL(fx - rx, fy - ry);
    out.dist1Nm = t;
    out.dist2Nm = s;
    return true;
}

}  // namespace plotter

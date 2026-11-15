/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "deviation.h"
#include "geo.h"
#include <cmath>

namespace plotter {

double angleDiffDeg(double a, double b) {
    double d = std::fmod(a - b, 360.0);
    if (d <= -180.0) d += 360.0;
    if (d > 180.0) d -= 360.0;
    return d;
}

double totalCorrection(double trueAzDeg, double compassBearingDeg) { return angleDiffDeg(trueAzDeg, compassBearingDeg); }

double deviationFromCt(double ctDeg, double declinationDeg) { return angleDiffDeg(ctDeg, declinationDeg); }

void DeviationCard::add(double hdg, double dev) {
    hdg = norm360(hdg);
    for (int i = 0; i < n; i++)
        if (std::fabs(angleDiffDeg(p[i].compassHdg, hdg)) < 5.0) { remove(i); break; }
    if (n == kMax) remove(0);
    p[n].compassHdg = (float)hdg;
    p[n].dev = (float)dev;
    n++;
    fit();
}

void DeviationCard::remove(int i) {
    if (i < 0 || i >= n) return;
    for (int k = i + 1; k < n; k++) p[k - 1] = p[k];
    n--;
    fit();
}

void DeviationCard::clear() { n = 0; fit(); }

// Minimos cuadrados con m terminos de la serie; false si el sistema sale mal condicionado (rumbos poco repartidos)
static bool solve(const DeviationPoint* p, int n, int m, double* c) {
    double a[5][6] = {};
    for (int i = 0; i < n; i++) {
        const double r = deg2rad(p[i].compassHdg);
        const double f[5] = {1.0, std::sin(r), std::cos(r), std::sin(2 * r), std::cos(2 * r)};
        for (int j = 0; j < m; j++) {
            for (int k = 0; k < m; k++) a[j][k] += f[j] * f[k];
            a[j][m] += f[j] * p[i].dev;
        }
    }
    for (int col = 0; col < m; col++) {
        int piv = col;
        for (int r = col + 1; r < m; r++) if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) piv = r;
        if (std::fabs(a[piv][col]) < 0.05 * n) return false;   // un termino casi sin informacion
        if (piv != col) for (int k = 0; k <= m; k++) { const double t = a[col][k]; a[col][k] = a[piv][k]; a[piv][k] = t; }
        for (int r = 0; r < m; r++) {
            if (r == col) continue;
            const double f = a[r][col] / a[col][col];
            for (int k = col; k <= m; k++) a[r][k] -= f * a[col][k];
        }
    }
    for (int j = 0; j < m; j++) c[j] = a[j][m] / a[j][j];
    return true;
}

void DeviationCard::fit() {
    for (double& c : coef) c = 0;
    terms = 0;
    rmsDeg = 0;
    if (n == 0) return;
    static const int tries[3] = {5, 3, 1};
    for (int m : tries) {
        if (n < m) continue;
        double c[5] = {0, 0, 0, 0, 0};
        if (!solve(p, n, m, c)) continue;
        bool sane = true;
        for (int j = 0; j < m; j++) if (std::fabs(c[j]) > 45.0) sane = false;
        if (!sane) continue;
        for (int j = 0; j < 5; j++) coef[j] = c[j];
        terms = m;
        break;
    }
    if (!terms) return;
    double s = 0;
    for (int i = 0; i < n; i++) { const double e = at(p[i].compassHdg) - p[i].dev; s += e * e; }
    rmsDeg = std::sqrt(s / n);
}

double DeviationCard::at(double hdg) const {
    const double r = deg2rad(hdg);
    return coef[0] + coef[1] * std::sin(r) + coef[2] * std::cos(r) + coef[3] * std::sin(2 * r) + coef[4] * std::cos(2 * r);
}

double DeviationCard::compassFromMagnetic(double mag) const {
    double ra = mag;
    for (int i = 0; i < 6; i++) ra = norm360(mag - at(ra));
    return ra;
}

}  // namespace plotter

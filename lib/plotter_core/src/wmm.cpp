/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wmm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "astro.h"
#include "wmm_cof.h"

namespace plotter {

namespace {
constexpr int kN = 12;
constexpr double kRef = 6371.2;                      // radio de referencia del modelo (km)
constexpr double kA = 6378.137, kF = 1 / 298.257223563;  // WGS84
constexpr double kD2R = 3.14159265358979323846 / 180.0;
}  // namespace

Wmm::Wmm() {
    std::memset(g_, 0, sizeof g_);
    std::memset(h_, 0, sizeof h_);
    std::memset(gd_, 0, sizeof gd_);
    std::memset(hd_, 0, sizeof hd_);
    int i = 0;
    for (int n = 1; n <= kN; n++)
        for (int m = 0; m <= n; m++, i++) {
            g_[n][m] = kWmm2025[i][0];
            h_[n][m] = kWmm2025[i][1];
            gd_[n][m] = kWmm2025[i][2];
            hd_[n][m] = kWmm2025[i][3];
        }
    epoch_ = kWmmEpoch;
    std::snprintf(name_, sizeof name_, "%s", kWmmName);
}

bool Wmm::load(const char* text) {
    if (!text) return false;
    double g[13][13] = {}, h[13][13] = {}, gd[13][13] = {}, hd[13][13] = {};
    double ep = 0;
    char nm[16] = "";
    int count = 0;
    bool first = true;
    const char* p = text;
    while (*p) {
        const char* e = std::strchr(p, '\n');
        size_t len = e ? size_t(e - p) : std::strlen(p);
        char line[96];
        if (len >= sizeof line) len = sizeof line - 1;
        std::memcpy(line, p, len);
        line[len] = 0;
        p = e ? e + 1 : p + len;
        if (std::strncmp(line, "9999", 4) == 0) break;
        if (first) {                                  // "    2025.0            WMM-2025     11/13/2024"
            char tmp[16];
            if (std::sscanf(line, "%lf %15s", &ep, tmp) != 2 || ep < 1900 || ep > 2200) return false;
            std::snprintf(nm, sizeof nm, "%s", tmp);
            first = false;
            continue;
        }
        int n, m;
        double a, b, c, d;
        if (std::sscanf(line, "%d %d %lf %lf %lf %lf", &n, &m, &a, &b, &c, &d) != 6) continue;
        if (n < 1 || n > kN || m < 0 || m > n) return false;
        g[n][m] = a;
        h[n][m] = b;
        gd[n][m] = c;
        hd[n][m] = d;
        count++;
    }
    if (first || count != 90) return false;
    std::memcpy(g_, g, sizeof g_);
    std::memcpy(h_, h, sizeof h_);
    std::memcpy(gd_, gd, sizeof gd_);
    std::memcpy(hd_, hd, sizeof hd_);
    epoch_ = ep;
    std::snprintf(name_, sizeof name_, "%s", nm);
    return true;
}

bool Wmm::field(double lat, double lon, double altKm, double year, MagField& out) const {
    if (!(lat >= -90 && lat <= 90) || !std::isfinite(lon) || !std::isfinite(year)) return false;
    // Geodesica -> geocentrica esferica
    const double e2 = kF * (2 - kF);
    const double sl = std::sin(lat * kD2R), cl = std::cos(lat * kD2R);
    const double rc = kA / std::sqrt(1 - e2 * sl * sl);
    const double pp = (rc + altKm) * cl, zz = (rc * (1 - e2) + altKm) * sl;
    const double r = std::sqrt(pp * pp + zz * zz);
    const double phic = std::asin(zz / r);
    const double x = std::sin(phic), c = std::cos(phic);      // x = cos(colatitud), c = sin(colatitud)
    const double dt = year - epoch_;

    // Legendre de Schmidt (semi-normalizadas) y su derivada
    double P[13][13] = {}, dP[13][13] = {};
    P[0][0] = 1;
    P[1][0] = x;
    P[1][1] = c;
    for (int n = 2; n <= kN; n++) P[n][n] = c * std::sqrt((2.0 * n - 1) / (2.0 * n)) * P[n - 1][n - 1];
    for (int m = 0; m <= kN; m++)
        for (int n = m + 1; n <= kN; n++) {
            if (n == 1) continue;                     // P[1][0] ya esta
            double a = std::sqrt(double(n * n - m * m));
            double b = n - 1 >= m ? std::sqrt(double((n - 1) * (n - 1) - m * m)) : 0;
            P[n][m] = ((2.0 * n - 1) * x * P[n - 1][m] - (n - 2 >= m ? b * P[n - 2][m] : 0)) / a;
        }
    // dP[n][m] = -dP/dphi = dP/dtheta, con sin(theta) dP/dtheta = n cos(theta) P[n][m] - sqrt(n^2 - m^2) P[n-1][m]
    // (la componente norte es X = -sum (a/r)^(n+2) (g cos + h sin) dP/dphi)
    for (int n = 1; n <= kN; n++)
        for (int m = 0; m <= n; m++) {
            double prev = (n - 1 >= m) ? std::sqrt(double(n * n - m * m)) * P[n - 1][m] : 0;
            double s = c > 1e-12 ? c : 1e-12;
            dP[n][m] = (n * x * P[n][m] - prev) / s;
        }

    double cm[13], sm[13];
    for (int m = 0; m <= kN; m++) {
        cm[m] = std::cos(m * lon * kD2R);
        sm[m] = std::sin(m * lon * kD2R);
    }
    double X = 0, Y = 0, Z = 0, ar = kRef / r, arn = ar * ar;
    for (int n = 1; n <= kN; n++) {
        arn *= ar;                                    // (a/r)^(n+2)
        double xs = 0, ys = 0, zs = 0;
        for (int m = 0; m <= n; m++) {
            double g = g_[n][m] + dt * gd_[n][m], h = h_[n][m] + dt * hd_[n][m];
            double t1 = g * cm[m] + h * sm[m];
            xs += t1 * dP[n][m];
            ys += m * (g * sm[m] - h * cm[m]) * P[n][m];
            zs += t1 * P[n][m];
        }
        X += arn * xs;
        Y += arn * ys;
        Z -= (n + 1) * arn * zs;
    }
    Y /= (c > 1e-12 ? c : 1e-12);
    // Esfericas -> geodesicas
    double psi = phic - lat * kD2R;
    double Xg = X * std::cos(psi) - Z * std::sin(psi);
    double Zg = X * std::sin(psi) + Z * std::cos(psi);
    out.xNt = Xg;
    out.yNt = Y;
    out.zNt = Zg;
    out.hNt = std::sqrt(Xg * Xg + Y * Y);
    out.fNt = std::sqrt(out.hNt * out.hNt + Zg * Zg);
    out.decDeg = std::atan2(Y, Xg) / kD2R;
    out.incDeg = std::atan2(Zg, out.hNt) / kD2R;
    return true;
}

double Wmm::declination(double lat, double lon, double year, double altKm) const {
    MagField f;
    return field(lat, lon, altKm, year, f) ? f.decDeg : 0;
}

const Wmm& defaultWmm() {
    static Wmm w;
    return w;
}

double decimalYear(int y, int m, int d, double utc) {
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return y + (dayOfYear(y, m, d) - 1 + utc / 24.0) / (leap ? 366.0 : 365.0);
}

double magneticDeclination(double lat, double lon, double year, double altKm) {
    return defaultWmm().declination(lat, lon, year, altKm);
}

}  // namespace plotter

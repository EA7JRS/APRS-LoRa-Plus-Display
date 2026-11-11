/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * Del nucleo de APRS LoRa+ Plotter (lib/plotter_core); solo cambia que no usa geo.h.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "astro.h"

#include <cmath>

namespace plotter {                 // lo que astro toma de geo.h del plotter
static constexpr double kPi = 3.14159265358979323846;
static inline double deg2rad(double d) { return d * kPi / 180.0; }
static inline double rad2deg(double r) { return r * 180.0 / kPi; }
static inline double norm360(double d) { d = std::fmod(d, 360.0); return d < 0 ? d + 360.0 : d; }
}

namespace plotter {

int dayOfYear(int y, int m, int d) {
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return cum[m - 1] + d + (leap && m > 2 ? 1 : 0);
}

// Ecuacion del tiempo (minutos) y declinacion (radianes) para la fraccion de anio g
static void solar(int y, int m, int d, double utcHours, double& eqMin, double& dec) {
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    double g = 2 * kPi / (leap ? 366.0 : 365.0) * (dayOfYear(y, m, d) - 1 + (utcHours - 12) / 24.0);
    eqMin = 229.18 * (0.000075 + 0.001868 * std::cos(g) - 0.032077 * std::sin(g) - 0.014615 * std::cos(2 * g) - 0.040849 * std::sin(2 * g));
    dec = 0.006918 - 0.399912 * std::cos(g) + 0.070257 * std::sin(g) - 0.006758 * std::cos(2 * g) + 0.000907 * std::sin(2 * g) -
          0.002697 * std::cos(3 * g) + 0.00148 * std::sin(3 * g);
}

double solarNoonUtcMin(int y, int m, int d, double lon) {
    double eq, dec;
    solar(y, m, d, 12, eq, dec);
    return 720 - 4 * lon - eq;
}

bool sunTimes(int y, int m, int d, double lat, double lon, double alt, double& rise, double& set) {
    double eq, dec;
    solar(y, m, d, 12, eq, dec);
    double la = deg2rad(lat);
    double c = (std::sin(deg2rad(alt)) - std::sin(la) * std::sin(dec)) / (std::cos(la) * std::cos(dec));
    if (c < -1 || c > 1) return false;
    double ha = rad2deg(std::acos(c)), noon = 720 - 4 * lon - eq;
    rise = noon - 4 * ha;
    set = noon + 4 * ha;
    return true;
}

void sunPosition(int y, int m, int d, double utc, double lat, double lon, double& alt, double& az) {
    double eq, dec;
    solar(y, m, d, utc, eq, dec);
    double ha = deg2rad((utc * 60 + eq + 4 * lon) / 4.0 - 180.0), la = deg2rad(lat);
    alt = rad2deg(std::asin(std::sin(la) * std::sin(dec) + std::cos(la) * std::cos(dec) * std::cos(ha)));
    az = norm360(rad2deg(std::atan2(-std::sin(ha) * std::cos(dec), std::sin(dec) * std::cos(la) - std::cos(dec) * std::sin(la) * std::cos(ha))));
}

static double julianDay(int y, int m, int d, double utc) {
    if (m <= 2) { y -= 1; m += 12; }
    int a = y / 100, b = 2 - a + a / 4;
    return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + b - 1524.5 + utc / 24.0;
}

double moonAgeDays(int y, int m, int d, double utc) {
    const double syn = 29.530588853, newMoon = 2451550.09766;       // luna nueva del 6 de enero de 2000
    double a = std::fmod(julianDay(y, m, d, utc) - newMoon, syn);
    return a < 0 ? a + syn : a;
}

double moonIllumination(double age) { return (1 - std::cos(2 * kPi * age / 29.530588853)) / 2; }

}  // namespace plotter

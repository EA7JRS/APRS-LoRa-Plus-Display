/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "chartui.h"

#include <cmath>
#include <cstdio>

namespace plotter {

Measure measure(LatLon a, LatLon b, bool rhumb, double speedKn) {
    Measure m;
    if (rhumb) {
        m.distanceNm = rhumbDistanceNm(a, b);
        m.bearing = rhumbBearing(a, b);
        m.reciprocal = norm360(m.bearing + 180.0);
    } else {
        m.distanceNm = gcDistanceNm(a, b);
        m.bearing = gcInitialBearing(a, b);
        m.reciprocal = gcInitialBearing(b, a);
    }
    if (speedKn > 0.05) m.hours = m.distanceNm / speedKn;
    return m;
}

double pathLengthNm(const LatLon* pts, int n, bool rhumb) {
    double d = 0;
    for (int i = 1; i < n; i++) d += rhumb ? rhumbDistanceNm(pts[i - 1], pts[i]) : gcDistanceNm(pts[i - 1], pts[i]);
    return d;
}

static int one(char* out, int size, double v, int degDigits, char pos, char neg, int decimals, bool comma) {
    char h = v < 0 ? neg : pos;
    v = std::fabs(v);
    double scale = std::pow(10.0, decimals);
    long total = std::lround(v * 60.0 * scale);                 // minutos en unidades del ultimo decimal: el redondeo no da 60,000
    long perDeg = long(60 * scale);
    long deg = total / perDeg, rem = total % perDeg;
    long whole = rem / long(scale), frac = rem % long(scale);
    int n;
    if (decimals > 0)
        n = std::snprintf(out, size_t(size), "%0*ld\xC2\xB0%02ld%c%0*ld\xE2\x80\xB2%c", degDigits, deg, whole, comma ? ',' : '.', decimals, frac, h);
    else
        n = std::snprintf(out, size_t(size), "%0*ld\xC2\xB0%02ld\xE2\x80\xB2%c", degDigits, deg, whole, h);
    return (n > 0 && n < size) ? n : 0;
}

int formatLatLon(char* out, int size, LatLon p, int decimals, bool comma) {
    if (decimals < 0) decimals = 0;
    if (decimals > 4) decimals = 4;
    int a = one(out, size, p.lat, 2, 'N', 'S', decimals, comma);
    if (!a || a + 1 >= size) return 0;
    out[a] = ' ';
    int b = one(out + a + 1, size - a - 1, p.lon, 3, 'E', 'W', decimals, comma);
    return b ? a + 1 + b : 0;
}

int formatDuration(char* out, int size, double hours) {
    if (hours < 0) return 0;
    long min = std::lround(hours * 60.0);
    int n;
    if (min < 60) n = std::snprintf(out, size_t(size), "%ld min", min);
    else if (min < 24 * 60) n = std::snprintf(out, size_t(size), "%02ld h %02ld min", min / 60, min % 60);
    else n = std::snprintf(out, size_t(size), "%ld d %02ld h", min / 1440, (min % 1440) / 60);
    return (n > 0 && n < size) ? n : 0;
}

void ViewHistory::push(const ChartView& v) {
    if (n_ > 0) {
        const ChartView& l = v_[n_ - 1];
        if (l.zoom == v.zoom && std::fabs(l.center.lat - v.center.lat) < 1e-7 && std::fabs(l.center.lon - v.center.lon) < 1e-7) return;
    }
    if (n_ == kMax) {
        for (int i = 1; i < kMax; i++) v_[i - 1] = v_[i];
        n_--;
    }
    v_[n_++] = v;
}

bool ViewHistory::back(ChartView& v) {
    if (n_ == 0) return false;
    v = v_[--n_];
    return true;
}

}  // namespace plotter

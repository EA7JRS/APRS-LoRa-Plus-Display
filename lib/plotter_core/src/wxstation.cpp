/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wxstation.h"

#include <cstdio>

namespace plotter {

static bool has(double v) { return v == v; }

int nearestStation(const WxObservation* obs, int n, LatLon here, double maxNm, bool requireWind, double* distNm) {
    int best = -1;
    double bd = maxNm;
    for (int i = 0; i < n; i++) {
        const WxObservation& o = obs[i];
        bool wind = has(o.windMs) && has(o.windDirDeg);
        bool any = wind || has(o.tempC) || has(o.pressureHpa);
        if (requireWind ? !wind : !any) continue;
        double d = gcDistanceNm(here, o.pos);
        if (d <= bd) { bd = d; best = i; }
    }
    if (best >= 0 && distNm) *distNm = bd;
    return best;
}

double observationAgeMin(const WxObservation& o, uint32_t nowUnix) {
    return (double(nowUnix) - double(o.timeUnix)) / 60.0;
}

int applyObservation(const WxObservation& o, uint32_t nowUnix, uint32_t nowMs, DataStore& st, double maxAgeMin) {
    double age = observationAgeMin(o, nowUnix);
    if (o.timeUnix == 0 || age > maxAgeMin || age < -10) return 0;
    const double kn = 3600.0 / 1852.0;
    int n = 0;
    auto put = [&](Key k, double v) { if (st.set(k, v, Source::Internet, nowMs, Quality::Estimated)) n++; };
    if (has(o.windMs) && o.windMs >= 0) put(Key::Tws, o.windMs * kn);
    if (has(o.windDirDeg) && has(o.windMs) && o.windMs > 0.2) put(Key::Twd, norm360(o.windDirDeg));     // en calma la direccion no vale
    if (has(o.gustMs) && o.gustMs >= 0) put(Key::Gust, o.gustMs * kn);
    if (has(o.pressureHpa) && o.pressureHpa > 850 && o.pressureHpa < 1090) put(Key::Pressure, o.pressureHpa);
    if (has(o.tempC) && o.tempC > -60 && o.tempC < 60) put(Key::AirTemp, o.tempC);
    if (has(o.humidityPct) && o.humidityPct >= 0 && o.humidityPct <= 100) put(Key::Humidity, o.humidityPct);
    if (has(o.visKm) && o.visKm >= 0) put(Key::Visibility, o.visKm / 1.852);                              // millas
    return n;
}

int describeObservation(char* out, int size, const WxObservation& o, LatLon here, uint32_t nowUnix) {
    static const char* const kDir[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    double d = gcDistanceNm(here, o.pos), b = gcInitialBearing(here, o.pos);
    long age = std::lround(observationAgeMin(o, nowUnix));
    if (age < 0) age = 0;
    long tenths = std::lround(d * 10);
    int n = std::snprintf(out, size_t(size), "Estaci\xC3\xB3n de tierra a %ld,%ld mn al %s \xC2\xB7 hace %ld min", tenths / 10, tenths % 10,
                          kDir[int(std::lround(b / 45.0)) % 8], age);
    return (n > 0 && n < size) ? n : 0;
}

}  // namespace plotter

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Calculos meteorologicos (ver wxcalc.h).
 */
#include "wxcalc.h"
#include <cmath>

namespace wxc {

int beaufort(double kn) {
    static const int top[] = {0, 3, 6, 10, 16, 21, 27, 33, 40, 47, 55, 63};       // limite superior de cada fuerza, en nudos
    long k = std::lround(kn < 0 ? 0 : kn);
    for (int f = 0; f < 12; f++) if (k <= top[f]) return f;
    return 12;
}
const char* beaufortName(int f) {
    static const char* n[] = {"Calm", "Light air", "Light breeze", "Gentle breeze", "Moderate breeze", "Fresh breeze", "Strong breeze",
                              "Near gale", "Gale", "Strong gale", "Storm", "Violent storm", "Hurricane force"};
    return n[f < 0 ? 0 : (f > 12 ? 12 : f)];
}

float dewPointC(float t, float rh) {
    if (!has(t) || !has(rh) || rh <= 0) return NA;
    if (rh > 100) rh = 100;
    const double a = 17.625, b = 243.04;
    const double g = std::log(rh / 100.0) + a * t / (b + t);
    return (float)(b * g / (a - g));
}
float windChillC(float t, float v) {
    if (!has(t) || !has(v) || t > 10 || v <= 4.8f) return NA;
    const double p = std::pow((double)v, 0.16);
    return (float)(13.12 + 0.6215 * t - 11.37 * p + 0.3965 * t * p);
}
float heatIndexC(float t, float rh) {
    if (!has(t) || !has(rh) || t < 27 || rh < 40) return NA;
    const double T = t * 9.0 / 5.0 + 32.0, R = rh;
    double hi = -42.379 + 2.04901523 * T + 10.14333127 * R - 0.22475541 * T * R - 0.00683783 * T * T - 0.05481717 * R * R
                + 0.00122874 * T * T * R + 0.00085282 * T * R * R - 0.00000199 * T * T * R * R;
    if (R < 13 && T >= 80 && T <= 112) hi -= (13 - R) / 4 * std::sqrt((17 - std::fabs(T - 95)) / 17);
    else if (R > 85 && T >= 80 && T <= 87) hi += (R - 85) / 10 * (87 - T) / 5;
    return (float)((hi - 32.0) * 5.0 / 9.0);
}
float feelsLikeC(float t, float rh, float v, int* kind) {
    if (kind) *kind = 0;
    float w = windChillC(t, v);
    if (has(w)) { if (kind) *kind = 1; return w; }
    float h = heatIndexC(t, rh);
    if (has(h)) { if (kind) *kind = 2; return h; }
    return NA;
}
float cloudBaseM(float t, float dew) {
    if (!has(t) || !has(dew) || dew > t) return NA;
    return (t - dew) * 125.0f;
}
float seaLevelHpa(float p, float alt, float t) {
    if (!has(p) || p <= 0) return NA;
    if (!has(t)) return (float)(p / std::pow(1.0 - 2.25577e-5 * alt, 5.25588));
    return (float)(p * std::pow(1.0 - 0.0065 * alt / (t + 0.0065 * alt + 273.15), -5.257));
}

void PressureTrend::add(uint32_t s, float hpa) {
    if (!has(hpa) || hpa <= 0) return;
    if (n_) {
        const int last = (head_ + n_ - 1) % N;
        if (s - t_[last] < 300) { p_[last] = hpa; return; }        // como mucho una cada 5 min: la ultima se actualiza
    }
    if (n_ == N) { head_ = (head_ + 1) % N; n_--; }
    const int i = (head_ + n_) % N;
    t_[i] = s; p_[i] = hpa; n_++;
}
bool PressureTrend::delta3h(uint32_t now, float& d) const {
    if (n_ < 2) return false;
    const int last = (head_ + n_ - 1) % N;
    if (now - t_[last] > 1800) return false;                       // la ultima lectura es vieja
    int ref = -1;                                                  // la mas cercana a hace 3 h, sin pasar de 3 h 20 min
    for (int k = 0; k < n_ - 1; k++) {
        const int i = (head_ + k) % N;
        const uint32_t age = t_[last] - t_[i];
        if (age > 12000) continue;
        ref = i;
        break;
    }
    if (ref < 0) return false;
    const uint32_t span = t_[last] - t_[ref];
    if (span < 3600) return false;                                 // con menos de 1 h no se dice nada
    d = (p_[last] - p_[ref]) * 10800.0f / (float)span;
    return true;
}
const char* PressureTrend::name(float d) {
    const float a = std::fabs(d);
    if (a < 1.0f) return "steady";
    if (a < 3.5f) return d > 0 ? "rising" : "falling";
    return d > 0 ? "rising fast" : "falling fast";
}

Unit distUnit(int s) { return s == 1 ? Unit{1 / 1.852, "nm"} : s == 2 ? Unit{1 / 1.609344, "mi"} : Unit{1, "km"}; }
Unit speedUnit(int s) { return s == 1 ? Unit{1 / 1.852, "kn"} : s == 2 ? Unit{1 / 1.609344, "mph"} : Unit{1, "km/h"}; }
float tempShown(float c, int s) { return s == 2 ? c * 9.0f / 5.0f + 32.0f : c; }
const char* tempLabel(int s) { return s == 2 ? "\xC2\xB0" "F" : "\xC2\xB0" "C"; }

} // namespace wxc

/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "windalarm.h"

#include <cmath>

namespace plotter {

static constexpr uint32_t kSpeedOn = 5000, kSpeedOff = 10000, kShiftOn = 10000;
static constexpr double kSmoothS = 30.0;       // la direccion se suaviza con una constante de 30 s

static double wrap180(double a) {
    a = std::fmod(a + 180.0, 360.0);
    if (a < 0) a += 360.0;
    return a - 180.0;
}

void WindAlarm::configure(const WindAlarmConfig& c) {
    if (c.shiftDeg != cfg_.shiftDeg) { refOk_ = false; smoothOk_ = false; sh_ = Level(); }
    if (c.twsMaxKn != cfg_.twsMaxKn) tws_ = Level();
    if (c.awsMaxKn != cfg_.awsMaxKn) aws_ = Level();
    cfg_ = c;
}

// Salta con el valor por encima del limite kSpeedOn seguidos; se apaga por debajo del limite menos la histeresis
// (5 %, al menos 1 kn) kSpeedOff seguidos. Sin dato, no cambia.
void WindAlarm::speed(Level& l, double v, double lim, uint32_t now) {
    if (lim <= 0) { l = Level(); return; }
    if (!std::isfinite(v)) { l.overMs = l.underMs = 0; return; }
    double hyst = lim * 0.05 < 1.0 ? 1.0 : lim * 0.05;
    if (!l.on) {
        if (v > lim) {
            if (!l.overMs) l.overMs = now ? now : 1;
            if (now - l.overMs >= kSpeedOn) { l.on = true; l.underMs = 0; }
        } else l.overMs = 0;
    } else {
        if (v < lim - hyst) {
            if (!l.underMs) l.underMs = now ? now : 1;
            if (now - l.underMs >= kSpeedOff) l = Level();
        } else l.underMs = 0;
    }
}

WindAlarmKind WindAlarm::update(uint32_t now, double tws, double aws, double twd) {
    twsV_ = tws;
    awsV_ = aws;
    speed(tws_, tws, cfg_.twsMaxKn, now);
    speed(aws_, aws, cfg_.awsMaxKn, now);
    if (cfg_.shiftDeg > 0 && std::isfinite(twd)) {
        double dt = lastMs_ && now > lastMs_ ? (now - lastMs_) / 1000.0 : 1.0;
        if (!smoothOk_) { smooth_ = twd; smoothOk_ = true; firstMs_ = now ? now : 1; }
        else smooth_ = std::fmod(smooth_ + wrap180(twd - smooth_) * (1.0 - std::exp(-dt / kSmoothS)) + 360.0, 360.0);
        if (!refOk_ && now - firstMs_ >= 30000) { ref_ = smooth_; refOk_ = true; }   // la referencia, tras 30 s de datos
        shift_ = refOk_ ? wrap180(smooth_ - ref_) : 0;
        Level& l = sh_;
        if (std::fabs(shift_) > cfg_.shiftDeg) {
            if (!l.overMs) l.overMs = now ? now : 1;
            if (!l.on && now - l.overMs >= kShiftOn) l.on = true;
        } else if (std::fabs(shift_) < cfg_.shiftDeg * 0.8) l = Level();
        else l.overMs = l.on ? l.overMs : 0;
    } else if (cfg_.shiftDeg <= 0) {
        sh_ = Level();
        shift_ = 0;
    }
    lastMs_ = now;
    active_ = tws_.on ? WindAlarmKind::Tws : aws_.on ? WindAlarmKind::Aws : sh_.on ? WindAlarmKind::Shift : WindAlarmKind::None;
    return active_;
}

bool WindAlarm::alerting(uint32_t now) const {
    if (snoozeUntil_ && (int32_t)(snoozeUntil_ - now) > 0) return false;
    switch (active_) {
        case WindAlarmKind::Tws: return !tws_.dismissed;
        case WindAlarmKind::Aws: return !aws_.dismissed;
        case WindAlarmKind::Shift: return !sh_.dismissed;
        default: return false;
    }
}

void WindAlarm::silence(uint32_t now, uint32_t ms) { snoozeUntil_ = (now + ms) ? now + ms : 1; }

void WindAlarm::dismiss(uint32_t now) {
    (void)now;
    switch (active_) {
        case WindAlarmKind::Tws: tws_.dismissed = true; break;
        case WindAlarmKind::Aws: aws_.dismissed = true; break;
        case WindAlarmKind::Shift:                                  // la direccion de ahora pasa a ser la referencia
            ref_ = smooth_;
            shift_ = 0;
            sh_ = Level();
            active_ = tws_.on ? WindAlarmKind::Tws : aws_.on ? WindAlarmKind::Aws : WindAlarmKind::None;
            break;
        default: break;
    }
}

double WindAlarm::value() const {
    switch (active_) {
        case WindAlarmKind::Tws: return twsV_;
        case WindAlarmKind::Aws: return awsV_;
        case WindAlarmKind::Shift: return shift_;
        default: return 0;
    }
}

double WindAlarm::limit() const {
    switch (active_) {
        case WindAlarmKind::Tws: return cfg_.twsMaxKn;
        case WindAlarmKind::Aws: return cfg_.awsMaxKn;
        case WindAlarmKind::Shift: return cfg_.shiftDeg;
        default: return 0;
    }
}

}  // namespace plotter

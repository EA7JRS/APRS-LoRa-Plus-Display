/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "route.h"

#include <cmath>
#include <cstdio>

namespace plotter {

void RouteFollower::start(const LatLon* pts, int n, LatLon here, int target) {
    st_ = RouteStatus();
    pts_ = pts;
    n_ = n;
    if (!pts || n < 1) return;
    if (target < 0) target = 0;
    if (target >= n) target = n - 1;
    st_.active = true;
    st_.target = target;
    origin_ = target > 0 ? pts[target - 1] : here;          // al primer waypoint se va desde donde se esta
    wasIn_ = false;
    minDist_ = 1e9;
}

void RouteFollower::advance(LatLon here) {
    (void)here;
    origin_ = pts_[st_.target];
    st_.target++;
    wasIn_ = false;
    minDist_ = 1e9;
    st_.switched = true;
}

bool RouteFollower::skip(LatLon here) {
    if (!st_.active || st_.target >= n_ - 1) return false;
    advance(here);
    st_.switched = false;
    return true;
}

const RouteStatus& RouteFollower::update(LatLon here, uint32_t now) {
    st_.switched = false;
    if (!st_.active) return st_;
    for (int pass = 0; pass < 2; pass++) {                  // segunda vuelta: valores del tramo nuevo tras cambiar
        LatLon wp = pts_[st_.target];
        st_.distanceNm = rhumb_ ? rhumbDistanceNm(here, wp) : gcDistanceNm(here, wp);
        st_.bearing = rhumb_ ? rhumbBearing(here, wp) : gcInitialBearing(here, wp);
        st_.legBearing = rhumb_ ? rhumbBearing(origin_, wp) : gcInitialBearing(origin_, wp);
        double along = 0;
        crossTrack(origin_, wp, here, st_.xteNm, along);
        st_.inArrival = st_.distanceNm <= radius_;
        // Por el traves: la demora al waypoint se aparta 90 grados o mas del rumbo con que se llega a el
        double legEnd = rhumb_ ? st_.legBearing : norm360(gcInitialBearing(wp, origin_) + 180.0);
        st_.abeam = gcDistanceNm(origin_, wp) > 1e-6 && std::fabs(norm180(st_.bearing - legEnd)) >= 90.0;
        if (st_.inArrival && !wasIn_) { wasIn_ = true; arrivalMs_ = now; minDist_ = st_.distanceNm; }
        if (wasIn_ && st_.distanceNm < minDist_) minDist_ = st_.distanceNm;

        bool go = false;
        switch (mode_) {
            case WpSwitch::Early: go = wasIn_ && uint32_t(now - arrivalMs_) >= earlyS_ * 1000u; break;
            case WpSwitch::Abeam: go = st_.abeam; break;
            case WpSwitch::Late: go = wasIn_ && st_.distanceNm > minDist_ + 0.005; break;      // ~9 m alejandose
        }
        if (st_.target >= n_ - 1) {                         // ultimo: no hay siguiente
            if (wasIn_ || (mode_ == WpSwitch::Abeam && st_.abeam)) st_.finished = true;
            break;
        }
        if (!go || pass == 1) break;
        advance(here);
    }
    return st_;
}

bool TrackSampler::sample(LatLon here, uint32_t now) {
    if (!any_) { any_ = true; prev_ = here; last_ = now; return true; }
    if (uint32_t(now - last_) < interval_) return false;
    double d = gcDistanceNm(prev_, here);
    if (d * kMetersPerNm < minDist_) return false;          // parado o fondeado: no se llena la tarjeta
    totalNm_ += d;
    prev_ = here;
    last_ = now;
    return true;
}

int depthLogLine(char* out, int size, const char* iso, LatLon here, double depthM) {
    int n = std::snprintf(out, size_t(size), "%s,%.6f,%.6f,%.1f\n", iso ? iso : "", here.lat, here.lon, depthM);
    return (n > 0 && n < size) ? n : 0;
}

AlertSound alertSound(AlertLevel l) {
    switch (l) {
        case AlertLevel::Alert: return {1, 0, false};       // informacion: un pitido (llegada a waypoint)
        case AlertLevel::Warn: return {2, 60, false};
        case AlertLevel::Alarm: return {3, 10, true};       // garreo, sonda, abordaje
        case AlertLevel::Emergency: return {5, 2, true};    // hombre al agua, socorro
        default: return {0, 0, false};
    }
}

}  // namespace plotter

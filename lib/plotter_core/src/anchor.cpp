/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "anchor.h"

#include "lights.h"

namespace plotter {

double AnchorWatch::suggestedRadiusM(double rodeM, double depthM, double bowHeightM, double marginM) {
    if (rodeM <= 0) return marginM;
    double swing = depthM > 0 ? swingRadiusM(rodeM, depthM, bowHeightM) : rodeM;
    if (swing <= 0) swing = rodeM;                    // la cadena no llega al fondo: dato dudoso, se toma entera
    return swing + marginM;
}

void AnchorWatch::drop(LatLon anchor, double radiusM, uint32_t nowMs) {
    st_ = AnchorState();
    st_.set = true;
    anchor_ = anchor;
    setRadius(radiusM);
    out_ = false;
    lastFixMs_ = nowMs;
}

void AnchorWatch::weigh() {
    st_ = AnchorState();
    out_ = false;
}

void AnchorWatch::setRadius(double radiusM) {
    if (radiusM < 5) radiusM = 5;
    if (radiusM > 500) radiusM = 500;
    radius_ = radiusM;
}

void AnchorWatch::update(LatLon boat, uint32_t nowMs) {
    if (!st_.set) return;
    lastFixMs_ = nowMs;
    st_.hasFix = true;
    st_.noFix = false;
    st_.distanceM = gcDistanceNm(anchor_, boat) * kMetersPerNm;
    st_.bearing = st_.distanceM > 0.5 ? gcInitialBearing(anchor_, boat) : 0;
    if (st_.distanceM > st_.maxDistanceM) st_.maxDistanceM = st_.distanceM;
    if (st_.distanceM > radius_) {
        if (!out_) { out_ = true; outSinceMs_ = nowMs; }
        if ((uint32_t)(nowMs - outSinceMs_) >= holdMs_) st_.dragging = true;
    } else {
        out_ = false;
        st_.dragging = false;
    }
}

void AnchorWatch::tick(uint32_t nowMs) {
    if (!st_.set) return;
    if ((uint32_t)(nowMs - lastFixMs_) >= fixTimeoutMs_) st_.noFix = true;
}

}  // namespace plotter

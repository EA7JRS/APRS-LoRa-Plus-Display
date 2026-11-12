/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Vigilancia de fondeo (alarma de garreo): circulo alrededor del ancla; avisa si el barco sale de el y se mantiene fuera,
// o si se queda sin posicion.
#ifndef PLOTTER_ANCHOR_H_
#define PLOTTER_ANCHOR_H_

#include <stdint.h>

#include "geo.h"

namespace plotter {

struct AnchorState {
    bool set = false;            // hay ancla marcada
    bool hasFix = false;         // ha llegado al menos una posicion desde que se fondeo
    double distanceM = 0;        // del ancla al barco
    double bearing = 0;          // demora del barco visto desde el ancla, verdadera
    double maxDistanceM = 0;     // la mayor distancia vista
    bool dragging = false;       // fuera del circulo durante el tiempo de confirmacion
    bool noFix = false;          // sin posicion durante demasiado tiempo
};

class AnchorWatch {
public:
    // Radio de alarma recomendado: borneo con la cadena filada mas un margen por el error del GPS y la eslora.
    // Sin fondo conocido (depthM <= 0) se toma toda la cadena.
    static double suggestedRadiusM(double rodeM, double depthM, double bowHeightM = 1.0, double marginM = 10.0);

    void drop(LatLon anchor, double radiusM, uint32_t nowMs);
    void weigh();
    void setRadius(double radiusM);                  // 5..500 m
    void setHoldMs(uint32_t ms) { holdMs_ = ms; }    // tiempo fuera del circulo antes de avisar (10 s)
    void setFixTimeoutMs(uint32_t ms) { fixTimeoutMs_ = ms; }   // sin posicion (60 s)
    void update(LatLon boat, uint32_t nowMs);        // con cada posicion
    void tick(uint32_t nowMs);                       // cada segundo, haya posicion o no

    const AnchorState& state() const { return st_; }
    LatLon anchor() const { return anchor_; }
    double radiusM() const { return radius_; }
    bool alarm() const { return st_.set && (st_.dragging || st_.noFix); }

private:
    AnchorState st_;
    LatLon anchor_{0, 0};
    double radius_ = 30;
    uint32_t holdMs_ = 10000, fixTimeoutMs_ = 60000;
    uint32_t outSinceMs_ = 0, lastFixMs_ = 0;
    bool out_ = false;
};

}  // namespace plotter
#endif

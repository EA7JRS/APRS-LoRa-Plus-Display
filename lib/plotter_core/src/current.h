/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Corriente y abatimiento. Rumbos verdaderos en grados; velocidades en nudos. Rumbo de superficie Rs = Rv + abatimiento
// (abatimiento + = a estribor, - = a babor). La corriente se da por su rumbo (hacia donde va, «set») e intensidad.
//  - currentFromVectors: corriente medida con los instrumentos = movimiento sobre el fondo (GPS) - movimiento en el agua
//    (rumbo y corredera).
//  - currentFromDr: corriente por la diferencia entre la estima (salida, rumbo, velocidad y tiempo) y la situacion.
//  - groundTrack: rumbo y velocidad efectivos con una corriente.
//  - courseToSteer: rumbo verdadero a gobernar para hacer un rumbo efectivo con una corriente.
// Sin Arduino.
#pragma once
#include <cstdint>

#include "geo.h"

namespace plotter {

struct Current {
    double setDeg = 0;      // hacia donde va
    double driftKn = 0;
};

Current currentFromVectors(double headingDeg, double stwKn, double leewayDeg, double cogDeg, double sogKn);
// dr: posicion de estima a la hora de la situacion (loxodromica). false si hours <= 0.
bool currentFromDr(LatLon start, double courseDeg, double speedKn, double leewayDeg, double hours, LatLon fix, LatLon& dr, Current& c);
void groundTrack(double headingDeg, double stwKn, double leewayDeg, const Current& c, double& cogDeg, double& sogKn);
// false si con esa corriente no se puede hacer ese rumbo (la corriente cruza mas rapido que el barco o lo hace retroceder)
bool courseToSteer(double cogDeg, double stwKn, double leewayDeg, const Current& c, double& headingDeg, double& sogKn);

// Media de la corriente medida (componentes norte y este con una constante de tiempo; un rumbo suelto no la mueve)
class CurrentAverager {
   public:
    explicit CurrentAverager(double tauS = 120) : tau_(tauS) {}
    void add(const Current& c, uint32_t nowMs);
    bool get(Current& c, uint32_t nowMs) const;      // false sin datos de los ultimos 30 s o con menos de 20 s de media
    void reset() { n_ = 0; }

   private:
    double tau_, e_ = 0, nn_ = 0;
    uint32_t first_ = 0, last_ = 0;
    int n_ = 0;
};

}  // namespace plotter

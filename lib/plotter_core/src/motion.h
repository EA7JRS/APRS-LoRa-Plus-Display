/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Movimiento relativo: CPA / TCPA, viento real, VMG, corriente. Velocidades en nudos, angulos en grados.
#ifndef PLOTTER_MOTION_H_
#define PLOTTER_MOTION_H_

#include "geo.h"

namespace plotter {

struct Mover { LatLon pos; double sogKn; double cogDeg; };

// Punto de maxima aproximacion. tcpaMin < 0: el blanco ya se aleja (cpaNm = distancia actual).
// false si no hay movimiento relativo (cpaNm = distancia actual, tcpaMin = 0).
bool cpa(const Mover& own, const Mover& target, double& cpaNm, double& tcpaMin);

// Viento real a partir del aparente. awaDeg: -180..180 (positivo = estribor), boatKn: velocidad del barco,
// headingDeg: proa verdadera. twaDeg -180..180, twdDeg 0..360 (de donde viene).
void trueWind(double awaDeg, double awsKn, double boatKn, double headingDeg, double& twaDeg, double& twsKn, double& twdDeg);

// Viento aparente a partir del real (para estimarlo con datos de internet)
void apparentWind(double twdDeg, double twsKn, double boatKn, double headingDeg, double& awaDeg, double& awsKn);

double vmgToWind(double boatKn, double twaDeg);                         // > 0 ganando barlovento
double vmgToWaypoint(double sogKn, double cogDeg, double bearingDeg);

// Corriente: de (proa, corredera) frente a (COG, SOG). setDeg = hacia donde va.
void current(double headingDeg, double stwKn, double cogDeg, double sogKn, double& setDeg, double& driftKn);

}  // namespace plotter
#endif

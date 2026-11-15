/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Situacion por demoras: dos demoras simultaneas a dos puntos conocidos, o dos demoras no simultaneas (demora trasladada)
// al mismo punto o a dos distintos, con el camino hecho entre las dos (rumbo efectivo y distancia). Demoras verdaderas,
// del barco al punto. Se resuelve en un plano local (navegacion costera: decenas de millas). Sin Arduino.
#pragma once

#include "geo.h"

namespace plotter {

struct BearingFix {
    LatLon fix{0, 0};        // situacion en el momento de la segunda demora
    LatLon first{0, 0};      // situacion en el momento de la primera (fix menos el camino)
    double cutDeg = 0;       // angulo de corte de las dos lineas (mejor cerca de 90)
    double dist1Nm = 0;      // distancia al punto 1 en el momento de la primera demora
    double dist2Nm = 0;      // distancia al punto 2 en el momento de la segunda
};

// false si las lineas son casi paralelas (corte < 10 grados) o se cortan detras de los puntos (demora equivocada)
bool fixFromBearings(LatLon mark1, double bearing1Deg, LatLon mark2, double bearing2Deg, double runCourseDeg, double runNm,
                     BearingFix& out);

}  // namespace plotter

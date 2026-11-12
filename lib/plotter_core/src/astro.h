/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Sol y luna. Formulas de NOAA (serie de Fourier): precision de alrededor de un minuto en latitudes medias.
// Pendiente: algoritmos de Meeus para el almanaque (fase 4).
#ifndef PLOTTER_ASTRO_H_
#define PLOTTER_ASTRO_H_

namespace plotter {

int dayOfYear(int y, int m, int d);

// Paso del sol por la altura altDeg (-0.833 orto/ocaso, -6 civil, -12 nautico, -18 astronomico).
// Minutos UTC del dia; pueden salir < 0 o > 1440 segun la longitud. false: no la cruza ese dia (dia o noche polar).
bool sunTimes(int y, int m, int d, double lat, double lon, double altDeg, double& riseUtcMin, double& setUtcMin);
double solarNoonUtcMin(int y, int m, int d, double lon);

// Altura y azimut del sol (grados; azimut desde el norte verdadero)
void sunPosition(int y, int m, int d, double utcHours, double lat, double lon, double& altDeg, double& azDeg);

// Edad de la luna en dias (0 = nueva) con el mes sinodico medio, y fraccion iluminada 0..1
double moonAgeDays(int y, int m, int d, double utcHours);
double moonIllumination(double ageDays);

}  // namespace plotter
#endif

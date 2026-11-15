/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Correccion total y desvio del compas. Ct = Zv - Za: azimut verdadero del astro (calculado con celnav) menos su demora
// de aguja. Desvio = Ct - dm (declinacion magnetica, del WMM). Signo nautico: + = E (la aguja marca de menos).
// Tablilla de desvios: observaciones (rumbo de aguja, desvio) ajustadas por minimos cuadrados a los coeficientes
// aproximados de Archibald Smith: d = A + B sen Ra + C cos Ra + D sen 2Ra + E cos 2Ra (con menos observaciones,
// A + B sen + C cos, o solo A). Sin Arduino.
#pragma once

namespace plotter {

double angleDiffDeg(double a, double b);                     // a - b, en (-180, 180]
double totalCorrection(double trueAzDeg, double compassBearingDeg);   // Ct = Zv - Za
double deviationFromCt(double ctDeg, double declinationDeg);          // desvio = Ct - dm

struct DeviationPoint {
    float compassHdg;        // rumbo de aguja del barco al observar, grados
    float dev;               // desvio medido, grados (+ E)
};

class DeviationCard {
public:
    static const int kMax = 24;
    DeviationPoint p[kMax];
    int n = 0;
    double coef[5] = {0, 0, 0, 0, 0};   // A, B, C, D, E
    int terms = 0;                       // 0 sin datos, 1 solo A, 3 A B C, 5 los cinco
    double rmsDeg = 0;                   // residuo de las observaciones frente a la curva

    // Anade una observacion; si ya hay una a menos de 5 grados del mismo rumbo, la sustituye; llena, se va la mas antigua
    void add(double compassHdgDeg, double devDeg);
    void remove(int i);
    void clear();
    void fit();                                     // recalcula coef, terms y rmsDeg
    double at(double compassHdgDeg) const;          // desvio para un rumbo de aguja
    double compassFromMagnetic(double magHdgDeg) const;   // rumbo de aguja para gobernar a un magnetico dado (Ra + d = Rm)
};

}  // namespace plotter

/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Sistemas de unidades: nautico, metrico, imperial y usuario. Valores internos: nm, kn, m, grados C, hPa, litros.
#ifndef PLOTTER_UNITS_H_
#define PLOTTER_UNITS_H_

namespace plotter {

enum class UnitSystem { Nautical, Metric, Imperial, User };
enum class DistUnit { Nm, Km, Mi };
enum class SpeedUnit { Kn, Kmh, Mph, Ms, Beaufort };
enum class DepthUnit { M, Ft, Fathom };
enum class TempUnit { C, F };
enum class PressUnit { Hpa, InHg, Mb };
enum class VolUnit { L, GalUs };

struct Units {
    DistUnit dist; SpeedUnit boatSpeed; SpeedUnit windSpeed; DepthUnit depth; DepthUnit height; TempUnit temp; PressUnit press; VolUnit vol;
};

Units unitsFor(UnitSystem s, const Units& user);      // User devuelve "user" tal cual

double distFromNm(double nm, DistUnit u);
double speedFromKn(double kn, SpeedUnit u);           // Beaufort devuelve la fuerza
double depthFromM(double m, DepthUnit u);
double tempFromC(double c, TempUnit u);
double pressFromHpa(double hpa, PressUnit u);
double volFromL(double l, VolUnit u);

const char* label(DistUnit u);
const char* label(SpeedUnit u);
const char* label(DepthUnit u);
const char* label(TempUnit u);
const char* label(PressUnit u);
const char* label(VolUnit u);

}  // namespace plotter
#endif

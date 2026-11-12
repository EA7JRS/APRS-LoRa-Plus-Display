/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "units.h"

#include "wind.h"

namespace plotter {

Units unitsFor(UnitSystem s, const Units& user) {
    switch (s) {
        case UnitSystem::Nautical: return {DistUnit::Nm, SpeedUnit::Kn, SpeedUnit::Kn, DepthUnit::M, DepthUnit::M, TempUnit::C, PressUnit::Hpa, VolUnit::L};
        case UnitSystem::Metric:   return {DistUnit::Km, SpeedUnit::Kmh, SpeedUnit::Ms, DepthUnit::M, DepthUnit::M, TempUnit::C, PressUnit::Hpa, VolUnit::L};
        case UnitSystem::Imperial: return {DistUnit::Mi, SpeedUnit::Mph, SpeedUnit::Mph, DepthUnit::Ft, DepthUnit::Ft, TempUnit::F, PressUnit::InHg, VolUnit::GalUs};
        default: return user;
    }
}

double distFromNm(double nm, DistUnit u) { return u == DistUnit::Km ? nm * 1.852 : (u == DistUnit::Mi ? nm * 1.852 / 1.609344 : nm); }

double speedFromKn(double kn, SpeedUnit u) {
    switch (u) {
        case SpeedUnit::Kmh: return kn * 1.852;
        case SpeedUnit::Mph: return kn * 1.852 / 1.609344;
        case SpeedUnit::Ms: return kn * 1852.0 / 3600.0;
        case SpeedUnit::Beaufort: return beaufort(kn);
        default: return kn;
    }
}

double depthFromM(double m, DepthUnit u) { return u == DepthUnit::Ft ? m / 0.3048 : (u == DepthUnit::Fathom ? m / 1.8288 : m); }
double tempFromC(double c, TempUnit u) { return u == TempUnit::F ? c * 9.0 / 5.0 + 32.0 : c; }
double pressFromHpa(double h, PressUnit u) { return u == PressUnit::InHg ? h / 33.8638866667 : h; }
double volFromL(double l, VolUnit u) { return u == VolUnit::GalUs ? l / 3.785411784 : l; }

const char* label(DistUnit u) { return u == DistUnit::Km ? "km" : (u == DistUnit::Mi ? "mi" : "nm"); }
const char* label(SpeedUnit u) {
    switch (u) { case SpeedUnit::Kmh: return "km/h"; case SpeedUnit::Mph: return "mph"; case SpeedUnit::Ms: return "m/s"; case SpeedUnit::Beaufort: return "Bft"; default: return "kn"; }
}
const char* label(DepthUnit u) { return u == DepthUnit::Ft ? "ft" : (u == DepthUnit::Fathom ? "brazas" : "m"); }
const char* label(TempUnit u) { return u == TempUnit::F ? "°F" : "°C"; }
const char* label(PressUnit u) { return u == PressUnit::InHg ? "inHg" : (u == PressUnit::Mb ? "mb" : "hPa"); }
const char* label(VolUnit u) { return u == VolUnit::GalUs ? "gal" : "L"; }

}  // namespace plotter

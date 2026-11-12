/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Modelo Magnetico Mundial (WMM): declinacion, inclinacion e intensidad del campo en cualquier punto.
// Lleva dentro el WMM2025 (valido hasta 2030.0); load() acepta un WMM.COF nuevo (el texto del fichero de NOAA)
// para cuando caduque. Algoritmo propio de sintesis armonica esferica (grado 12), como describe el informe del WMM.
#ifndef PLOTTER_WMM_H_
#define PLOTTER_WMM_H_

namespace plotter {

struct MagField {
    double decDeg = 0;      // declinacion (+ = al este del norte verdadero): rumbo verdadero = magnetico + dec
    double incDeg = 0;      // inclinacion (+ hacia abajo)
    double hNt = 0, fNt = 0;  // intensidad horizontal y total (nT)
    double xNt = 0, yNt = 0, zNt = 0;  // norte, este y abajo (nT)
};

class Wmm {
public:
    Wmm();                                  // con el WMM2025
    bool load(const char* cofText);         // false: texto no valido (se queda el modelo anterior)
    double epoch() const { return epoch_; }
    double validUntil() const { return epoch_ + 5; }
    const char* name() const { return name_; }
    bool expired(double year) const { return year >= validUntil(); }
    // lat / lon geodesicas (WGS84), altitud sobre el elipsoide en km, anio decimal. false fuera de -90..90.
    bool field(double latDeg, double lonDeg, double altKm, double year, MagField& out) const;
    double declination(double latDeg, double lonDeg, double year, double altKm = 0) const;

private:
    double g_[13][13], h_[13][13], gd_[13][13], hd_[13][13];
    double epoch_;
    char name_[16];
};

const Wmm& defaultWmm();
// Anio decimal de una fecha UTC (2026-07-02 12:00 = 2026.5)
double decimalYear(int y, int m, int d, double utcHours = 0);
// Declinacion con el modelo de serie (grados, + este)
double magneticDeclination(double latDeg, double lonDeg, double year, double altKm = 0);

}  // namespace plotter
#endif

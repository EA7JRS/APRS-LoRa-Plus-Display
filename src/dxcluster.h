/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// DX Cluster: lineas "DX de" (en directo) y de sh/dx, banda, modo y actividades, como core/DxCluster.kt de la app. C++ puro.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dx {

struct Spot {
    std::string spotter, call, comment, time, band, mode, activity;   // mode: CW, PHO, DIG; activity: POTA, SOTA, IOTA, WWFF, DME
    double khz = 0;
    bool skimmer = false;               // RBN (spotter con -#)
    uint32_t atMs = 0;
};

// Analiza una linea; false si no es un spot
bool parse(const std::string& line, Spot& out);
// Banda (160m ... 70cm) de una frecuencia en kHz; "" si no es de aficionado
std::string band(double khz);
// Modo por el comentario (CW, FT8, SSB...) o por el plan de bandas de la IARU R1
std::string mode(double khz, const std::string& comment);

struct Filter {
    bool human = true, rbnCw = true, rbnDig = true;     // origen
    bool cw = true, pho = true, dig = true;             // modo
    std::string band;                                   // "" = todas
    std::string activity;                               // "" = todas
};
bool pass(const Filter& f, const Spot& s);

} // namespace dx

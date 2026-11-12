/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Radioavisos nauticos en vigor: lector del XML publico del Instituto Hidrografico de la Marina (NAVAREA III,
// https://armada.defensa.gob.es/ihm/XML/navareas_crudo.xml) y extraccion de las posiciones que van dentro del texto.
#ifndef PLOTTER_NAVWARN_H_
#define PLOTTER_NAVWARN_H_

#include <string>
#include <vector>

#include "geo.h"

namespace plotter {

struct NavWarning {
    std::string number;             // "0086/26"
    std::string date;               // "2026-06-03" (emision)
    std::string areaEs, areaEn;     // zona
    std::string titleEs, titleEn;
    std::string textEs, textEn;
    std::vector<LatLon> positions;  // sacadas del texto, en el orden en que aparecen
};

constexpr const char* kIhmNavareaUrl = "https://armada.defensa.gob.es/ihm/XML/navareas_crudo.xml";

// Lee el XML del IHM. "generated" recibe la fecha del fichero ("2026-11-10T09:31:41"), si se pide. false si no es ese formato.
bool navWarningsParseIhm(const std::string& xml, std::vector<NavWarning>& out, std::string* generated = nullptr);

// Posiciones escritas como en los radioavisos (y en Navtex): grados y minutos con decimales, "46-04.0N 033-12.8E",
// "42 11,718N 009 59,778E", "36-31N 006-17W". Devuelve cuantas ha anadido a "out".
int navWarningPositions(const std::string& text, std::vector<LatLon>& out);

// Distancia en millas del barco a la posicion mas cercana del aviso; negativa si el aviso no trae posiciones.
double navWarningDistanceNm(const NavWarning& w, LatLon here);

}  // namespace plotter
#endif

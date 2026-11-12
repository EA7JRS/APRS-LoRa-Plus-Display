/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// GPX 1.1: waypoints, rutas y tracks, para intercambiarlos con OpenCPN y otros programas.
// Lector tolerante: ignora lo que no conoce (extensiones, metadatos). Pensado para archivos que caben en memoria.
#ifndef PLOTTER_GPX_H_
#define PLOTTER_GPX_H_

#include <string>
#include <vector>

namespace plotter {

struct GpxPoint {
    double lat = 0, lon = 0;
    std::string name;
    std::string time;       // ISO 8601, vacio si no hay
};

struct GpxPath {            // una ruta o un track (los segmentos de un track se unen)
    std::string name;
    std::vector<GpxPoint> pts;
};

struct GpxData {
    std::vector<GpxPoint> waypoints;
    std::vector<GpxPath> routes;
    std::vector<GpxPath> tracks;
};

bool gpxParse(const std::string& xml, GpxData& out);       // false si no es GPX o hay coordenadas no validas
std::string gpxWrite(const GpxData& d);

}  // namespace plotter
#endif

/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Observacion de la estacion meteorologica mas cercana (AEMET OpenData, "observacion convencional"): eleccion de la estacion
// y paso de sus datos a la tabla del plotter. El JSON lo lee el firmware; aqui llegan ya los numeros.
// Campos de AEMET (de memoria, por comprobar con una respuesta real): idema, ubi, lat, lon, fint (hora), ta (grados C),
// hr (%), pres (hPa), vv (viento medio, m/s), dv (direccion de donde viene, grados), vmax (racha, m/s), vis (km), prec (mm).
#ifndef PLOTTER_WXSTATION_H_
#define PLOTTER_WXSTATION_H_

#include <cmath>
#include <cstdint>

#include "geo.h"
#include "store.h"

namespace plotter {

struct WxObservation {
    char id[10] = "";               // idema
    char name[32] = "";             // ubi
    LatLon pos{0, 0};
    uint32_t timeUnix = 0;          // fint
    // NAN = la estacion no da ese dato
    double tempC = NAN, humidityPct = NAN, pressureHpa = NAN, windMs = NAN, windDirDeg = NAN, gustMs = NAN, visKm = NAN, rainMm = NAN;
};

// Indice de la estacion mas cercana que tenga viento, a no mas de "maxNm" millas; -1 si no hay. "requireWind" = false
// acepta cualquiera con algun dato.
int nearestStation(const WxObservation* obs, int n, LatLon here, double maxNm = 30.0, bool requireWind = true, double* distNm = nullptr);

// Antiguedad en minutos (negativa si la hora de la observacion es posterior a "nowUnix")
double observationAgeMin(const WxObservation& o, uint32_t nowUnix);

// Pasa la observacion a la tabla como fuente Internet y calidad "estimado" (es una estacion de tierra, no el barco).
// No escribe datos con mas de "maxAgeMin" minutos. Devuelve cuantos valores ha puesto.
int applyObservation(const WxObservation& o, uint32_t nowUnix, uint32_t nowMs, DataStore& store, double maxAgeMin = 90.0);

// "Estacion de tierra a 4,2 mn al NE · hace 25 min"
int describeObservation(char* out, int size, const WxObservation& o, LatLon here, uint32_t nowUnix);

}  // namespace plotter
#endif

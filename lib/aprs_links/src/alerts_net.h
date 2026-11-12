/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Avisos oficiales por la WiFi de la pantalla (plotter, APRS LoRa+ Display), en una tarea propia para no frenar la red de
 * datos: meteorologicos (RSS de Meteoalerta de AEMET en Espana, MeteoAlarm en el resto de Europa, NWS en EE. UU.),
 * tsunamis (tsunami.gov), terremotos (USGS, EMSC) y tiempo espacial (SWPC). Modelo y lectores del nucleo (alerts.h); los
 * JSON grandes se leen con ArduinoJson y un filtro, los XML por bloques (XmlBlockSplitter) sin guardar el documento.
 * Solo se compila con -D APRS_LINKS_ALERTS (el rotor no lo lleva).
 */
#pragma once
#include <Arduino.h>
#include <vector>

#include "alerts.h"

namespace alertsnet {

struct Config {
    bool on = false;
    char zones[64] = "";        // MeteoAlarm / RSS de AEMET (sin poligonos): palabras de la zona separadas por comas; vacio = todas
    char country[3] = "";       // ISO; vacio = por la posicion (Espana y EE. UU.; para MeteoAlarm hay que ponerlo)
    bool weather = true, quakes = true, tsunami = true, space = true;
    int minutes = 15;
};

struct Status {
    uint32_t ok = 0, fail = 0, lastMs = 0;
    char msg[72] = "";
};

void begin();
void apply(const Config& c);
void setPosition(double lat, double lon);
void refresh();                                             // descargar ya
// true una vez por cada descarga nueva: los avisos en vigor (todas las fuentes) y el tiempo espacial
bool take(std::vector<plotter::Alert>& out, plotter::SpaceWx& space);
const Status& status();
// Pais por la posicion (cajas aproximadas: "ES", "US"; "" si no se sabe)
const char* countryOf(double lat, double lon);

}  // namespace alertsnet

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Datos de la red local, con la misma conectividad que el plotter (lib/aprs_links y lib/plotter_core): WiFi cliente, un
 * servidor TCP (NMEA 0183, gpsd o Signal K), escucha UDP (NMEA, p. ej. el instrumento de viento) y el tiempo de
 * OpenWeatherMap. Todo entra en una tabla de datos con prioridad de fuentes; la posicion del tracker (BLE) entra tambien,
 * por debajo de la red. Los blancos AIS que lleguen por la red van a su tabla, con distancia y CPA.
 */
#pragma once
#include <cstdint>

#include "aistargets.h"
#include "net.h"
#include "store.h"

namespace boat {

struct Stats {
    uint32_t tcpLines = 0, udpLines = 0, aisMessages = 0, skValues = 0;
    uint32_t netLastMs = 0;            // ultima linea recibida por la red
    uint32_t wxMs = 0;                 // ultima observacion de internet (0 = ninguna)
    char wxPlace[32] = "";
    // ultimo aviso de Signal K (notifications.*)
    uint32_t skSeq = 0;
    char skKey[48] = "", skState[12] = "", skMessage[96] = "";
};

void begin();                          // lee la configuracion guardada y arranca la red (tarea aparte)
void loop(uint32_t nowMs);             // llamar a menudo desde loop()
const net::Config& config();
void setConfig(const net::Config& c);  // guarda y vuelve a conectar
// Posicion del tracker por BLE (solo con fix): entra en la tabla como fuente BLE
void trackerFix(double lat, double lon, double sogKn, double cogDeg, uint32_t nowMs);

plotter::DataStore& store();
plotter::AisTargets& targets();
const Stats& stats();
const char* sourceName(plotter::Source s);   // etiqueta corta: "LAN" (red), "TRK" (tracker), "WEB" (internet)...
const char* kindName(int tcpKind);           // "Off", "NMEA 0183", "gpsd", "Signal K"
int defaultPort(int tcpKind);                // 10110, 2947, 8375
}

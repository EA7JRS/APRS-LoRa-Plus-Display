/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// NMEA 0183: comprobacion, campos y las tramas de la fase 1 (RMC, GGA, GLL, VTG, GSV, GSA, DPT, DBT, MWV, HDG, HDT, HDM, VHW, MTW, XDR, RSA, VDM, VDO).
#ifndef PLOTTER_NMEA_H_
#define PLOTTER_NMEA_H_

#include <cstdint>

#include "ais.h"
#include "gnss.h"
#include "store.h"

namespace plotter {

uint8_t nmeaChecksum(const char* body);         // XOR de lo que va entre '$' o '!' y '*'
bool nmeaChecksumOk(const char* line);          // false tambien si falta la suma

// Las tramas pueden llegar con un bloque de etiquetas delante (\\s:estacion,c:hora*XX\\!AIVDM...), como las de AIS-catcher y
// AIS Dispatcher. Devuelve el comienzo de la trama ('$' o '!'), o la propia linea si no hay bloque.
const char* nmeaSentenceStart(const char* line);

struct NmeaStats {
    uint32_t good = 0;
    uint32_t badChecksum = 0;       // suma mal o trama cortada
    uint32_t malformed = 0;
    uint32_t aisRejected = 0;       // AIS corto, MMSI 0 o fragmentos sueltos
    uint32_t aisUnsupported = 0;    // tipo de mensaje que no se usa
    uint32_t aisOversized = 0;      // aceptado, con mas bits de la cuenta
};

// Trama APB para un piloto automatico (Pypilot y la mayoria de pilotos con entrada NMEA 0183): error de rumbo en millas
// (positivo = el barco esta a estribor de la ruta, hay que gobernar a babor), demoras verdaderas o magneticas segun "magnetic".
// Devuelve la longitud escrita (con "*XX\r\n"), 0 si no cabe.
int nmeaApb(char* out, int size, double xteNm, bool arrived, bool perpendicularPassed, double bearingOriginToDest,
            const char* waypointId, double bearingToDest, double headingToSteer, bool magnetic);

// Trama RMB (datos minimos de navegacion al waypoint): desvio y hacia donde gobernar, waypoints de origen y destino, posicion
// del destino, distancia, demora verdadera, velocidad de acercamiento y llegada. Devuelve la longitud escrita, 0 si no cabe.
int nmeaRmb(char* out, int size, double xteNm, const char* originId, const char* destId, double destLat, double destLon,
            double rangeNm, double bearingTrue, double closingKn, bool arrived);

enum class NmeaResult { Bad, Ignored, Data, Ais };

class Nmea0183 {
   public:
    // Procesa una linea. Los datos van a "store" con la fuente indicada; un mensaje AIS completo queda en ais().
    NmeaResult feed(const char* line, Source src, uint32_t nowMs, DataStore& store);
    const AisReport& ais() const { return ais_; }
    const NmeaStats& stats() const { return stats_; }
    GnssSky& sky() { return sky_; }
    // Del bloque de etiquetas de la ultima linea: hora UNIX (0 si no venia) y estacion ("" si no venia)
    uint32_t tagTime() const { return tagTime_; }
    const char* tagSource() const { return tagSource_; }

   private:
    AisDecoder dec_;
    AisReport ais_;
    NmeaStats stats_;
    GnssSky sky_;
    uint32_t tagTime_ = 0;
    char tagSource_[16] = "";
};

}  // namespace plotter
#endif

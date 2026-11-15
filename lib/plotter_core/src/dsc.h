/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// LSD / DSC (ITU-R M.493): llamadas que llegan ya decodificadas en la sentencia DSC de NMEA 0183, la que saca una emisora con
// LSD (Icom IC-M423GE / IC-M330GE...) o la app APRS LoRa+ con su decodificador del canal 70 y de MF / HF:
// $CDDSC,formato,MMSI que llama (10 cifras),categoria,naturaleza o telecomando 1,comunicacion siguiente o telecomando 2,
//        posicion (10 cifras: cuadrante, latitud, longitud) o frecuencia,hora hhmm,MMSI del buque en peligro,naturaleza,R/B/S,E*hh
// Los simbolos van como sus dos ultimas cifras (112 -> "12"). Ademas: nombres en espanol y lista de las ultimas llamadas.
#ifndef PLOTTER_DSC_H_
#define PLOTTER_DSC_H_

#include <cstdint>
#include <string>
#include <vector>

#include "geo.h"

namespace plotter {

struct DscCall {
    int format = -1;               // 2 socorro, 16 todos los buques, 20 individual, 14 grupo, 02 zona, 23 automatica (sin el 1 de delante)
    std::string mmsi;              // estacion que llama (9 cifras; en una alerta de socorro, el buque en peligro)
    int category = -1;             // 0 rutina, 8 seguridad, 10 urgencia, 12 socorro
    int f4 = -1;                   // alerta de socorro: naturaleza; si no, telecomando 1 (10 acuse de socorro, 12 retransmision)
    int f5 = -1;                   // alerta: comunicacion siguiente; si no, telecomando 2
    std::string field6;            // posicion o frecuencia, tal como llega
    bool hasPos = false;
    LatLon pos{0, 0};
    std::string time;              // "hhmm" UTC de la posicion, vacio si no viene
    std::string distressMmsi;      // retransmisiones y acuses: el buque en peligro
    int distressNature = -1;
    char ack = 'S';                // R se pide acuse, B acuse, S fin
    int64_t utc = 0;               // hora de recepcion (UNIX, 0 = no se sabe)
    int repeats = 1;               // veces que ha llegado la misma llamada (10 min)

    bool distress() const { return format == 12 || category == 12; }
    // El buque al que se refiere: el que llama en una alerta, el nombrado en una retransmision o un acuse
    const std::string& ship() const { return format == 12 || distressMmsi.empty() ? mmsi : distressMmsi; }
    int nature() const { return format == 12 ? f4 : distressNature; }
    std::string key() const;
};

// Lee una sentencia DSC (con o sin '$', con o sin suma; si la trae, ha de cuadrar). false si no es DSC o esta mal.
bool parseDscSentence(const char* line, DscCall& out);
// Las 10 cifras de posicion; false con "9999999999" o cifras imposibles
bool dscPosition(const std::string& digits, LatLon& out);

const char* dscFormatEs(int f);
const char* dscCategoryEs(int c);
const char* dscNatureEs(int n);
const char* dscTelecommandEs(int t);
const char* dscMmsiKindEs(const std::string& mmsi);   // costera, grupo, buque, SART, MOB, RLS...

// Ultimas llamadas, la mas nueva primero; la misma llamada en 10 min solo suma una repeticion
class DscLog {
public:
    static const size_t kMax = 60;
    bool add(const DscCall& c);              // true si es nueva (no una repeticion)
    const std::vector<DscCall>& items() const { return items_; }
    uint32_t seq() const { return seq_; }
    void clear() { items_.clear(); seq_++; }
private:
    std::vector<DscCall> items_;
    uint32_t seq_ = 0;
};

}  // namespace plotter

#endif

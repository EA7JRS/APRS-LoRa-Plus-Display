/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// NAVTEX: mensajes que llegan ya decodificados por NMEA 0183 en sentencias NRX (un receptor NAVTEX de a bordo, o la app
// APRS LoRa+ con su decodificador por el servidor NMEA del movil). Dos formas: la de los receptores Furuno
// ($CRNRX,TA21,linea,lineas,518,dia,mes,anio,hhmmss,error,texto: una linea de mensaje por sentencia; si acaba en espacio,
// la siguiente la continua: asi parte la app las lineas largas) y la de IEC 61162-1
// ($CRNRX,total,num,secuencia,TA21,indice frecuencia,hhmmss.ss,dia,mes,anio,caracteres,malos,A/V,cuerpo con ^hh).
// Ademas: asuntos (letra B2), horario de cada estacion (letra B1 x 10 min, cada 4 h) y lista de los ultimos mensajes.
#ifndef PLOTTER_NAVTEX_H_
#define PLOTTER_NAVTEX_H_

#include <cstdint>
#include <string>
#include <vector>

#include "geo.h"

namespace plotter {

struct NavtexMessage {
    char station = '?';             // B1
    char subject = '?';             // B2
    int number = 0;                 // B3B4
    double khz = 0;                 // 518, 490, 4209.5 (0 = no se sabe)
    std::string text;               // lineas separadas por '\n'
    int64_t utc = 0;                // hora de recepcion (UNIX, 0 = no viene)
    bool complete = true;           // todas las lineas y estado A
    double errorPct = -1;           // tasa de error del receptor (-1 = no viene)
    std::vector<LatLon> positions;  // sacadas del texto (navWarningPositions)
    bool urgent() const { return subject == 'D'; }   // busqueda y salvamento, socorro, pirateria
    std::string id() const;         // "GA05"
};

const char* navtexSubjectEs(char b2);
const char* navtexSubjectEn(char b2);
int navtexFirstSlotMin(char b1);                    // minutos UTC de la primera emision (A = 0 ... X = 230); -1 si no es letra de estacion
int64_t navtexNextSlot(char b1, int64_t nowUnix);   // UNIX de la siguiente emision (la que esta en curso cuenta 10 min); -1 si no hay

// Monta mensajes a partir de sentencias NRX (con o sin "$", con o sin suma). feed() devuelve true cuando hay un mensaje en take().
class NrxAssembler {
public:
    bool feed(const char* sentence);
    bool take(NavtexMessage& out);
private:
    struct Part { std::string key; int total = 0; std::vector<std::string> lines; NavtexMessage m; uint32_t age = 0; };
    std::vector<Part> parts_;
    std::vector<NavtexMessage> ready_;
    uint32_t tick_ = 0;
    Part& part(const std::string& key);
};

// Ultimos mensajes, el mas nuevo primero; uno por estacion + id + frecuencia (la repeticion sustituye a la copia anterior)
class NavtexLog {
public:
    static const size_t kMax = 60;
    void add(const NavtexMessage& m);
    const std::vector<NavtexMessage>& items() const { return items_; }
    uint32_t seq() const { return seq_; }           // cambia con cada mensaje
    void clear() { items_.clear(); seq_++; }
private:
    std::vector<NavtexMessage> items_;
    uint32_t seq_ = 0;
};

}  // namespace plotter
#endif

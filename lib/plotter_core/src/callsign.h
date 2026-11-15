/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Comprobacion del indicativo de radioaficionado en RadioID.net (DMR ID / CCS7), como la app Android (CallsignCheck.kt,
// Licence.kt): un indicativo con DMR ID o CCS7 ha pasado por la comprobacion de licencia de RadioID. Sin indicativo validado,
// el plotter no manda al transponder ordenes que cambien lo que emite (datos del barco).
// API: GET https://database.radioid.net/api/dmr/user/?callsign=EA7JRS -> {"count":n,"results":[{"id":2147001,
// "callsign":"EA7JRS","fname":"...","surname":"...","country":"Spain"}, ...]} (un resultado por DMR ID).
#ifndef PLOTTER_CALLSIGN_H_
#define PLOTTER_CALLSIGN_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace plotter {
namespace callsign {

constexpr int64_t RECHECK_S = 90LL * 24 * 3600;                 // se vuelve a comprobar a los 90 dias

// "ea7jrs-7" -> "EA7JRS" en out; false si no parece un indicativo (NOCALL, N0CALL, solo letras...)
bool base(const char* call, char* out, size_t size);
std::string url(const char* base);

struct Result {
    bool     found = false;
    uint32_t ids[8] = {};
    int      nIds = 0;
    char     name[48] = "";
    char     country[32] = "";
    bool has(uint32_t id) const;
    void idsText(char* out, size_t size) const;                 // "2147001,2147002"
};
// Respuesta de RadioID: solo cuenta un resultado con exactamente ese indicativo. false si no es una respuesta valida
bool parse(const std::string& json, const char* base, Result& out);

// Lo guardado tras validar
struct Licence {
    char    call[10] = "";
    char    ids[64] = "";
    char    name[48] = "";
    char    country[32] = "";
    int64_t atUnix = 0;                                         // 0 = sin hora cuando se valido
};
bool valid(const char* call, const Licence& l);                 // el indicativo (con o sin SSID) es el validado
bool needsRecheck(const Licence& l, int64_t nowUnix);           // validado hace mas de 90 dias (o sin hora)
// Resultado de una consulta: nullptr si vale (y rellena l), o el motivo en espanol
const char* check(const char* call, const Result* r, uint32_t expectedId, int64_t nowUnix, Licence& l);

}  // namespace callsign
}  // namespace plotter
#endif

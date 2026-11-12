/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Entrada NMEA 2000: identificador CAN -> PGN, montaje de mensajes de varias tramas ("fast packet") y lectura de los PGN
// de navegacion, viento, motor y AIS. Solo escucha. Posicion y tamano de cada campo contrastados con la base de datos de
// canboat (Apache-2.0), cuyos ejemplos sirven de prueba.
#ifndef PLOTTER_N2K_H_
#define PLOTTER_N2K_H_

#include <stdint.h>

#include "ais.h"
#include "store.h"

namespace plotter {

struct N2kId { uint8_t prio = 0, src = 0, dst = 255; uint32_t pgn = 0; };
N2kId n2kParseId(uint32_t canId);                 // identificador extendido de 29 bits
bool n2kIsFastPacket(uint32_t pgn);               // de los PGN que se leen aqui, los que llegan en varias tramas

// Monta mensajes de varias tramas: la primera lleva secuencia y longitud total, las demas secuencia y numero de trama.
class N2kFastPacket {
public:
    static constexpr int kMaxLen = 223;
    // true cuando la trama completa un mensaje: queda en data() / length() hasta la siguiente llamada
    bool feed(uint32_t pgn, uint8_t src, const uint8_t* d, int len, uint32_t nowMs);
    const uint8_t* data() const { return done_; }
    int length() const { return doneLen_; }
    uint32_t dropped() const { return dropped_; } // mensajes empezados que no se completaron

private:
    struct Slot { uint32_t pgn = 0; uint8_t src = 0, seq = 0, next = 0; int total = 0, have = 0; uint32_t t = 0; bool used = false; uint8_t buf[kMaxLen]; };
    Slot s_[6];
    uint8_t done_[kMaxLen];
    int doneLen_ = 0;
    uint32_t dropped_ = 0;
};

enum class N2kResult : uint8_t { None, Pending, Data, Ais, Unknown };

struct N2kStats { uint32_t frames = 0, messages = 0, known = 0, ais = 0; };

class N2kReader {
public:
    // Una trama CAN (identificador extendido, hasta 8 bytes)
    N2kResult feedFrame(uint32_t canId, const uint8_t* d, int len, Source src, uint32_t nowMs, DataStore& store);
    // Un mensaje ya montado
    N2kResult feedMessage(uint32_t pgn, const uint8_t* d, int len, Source src, uint32_t nowMs, DataStore& store);
    const AisReport& ais() const { return ais_; }  // valido tras devolver Ais
    const N2kStats& stats() const { return stats_; }
    uint32_t dropped() const { return fast_.dropped(); }
    // Hora UTC del bus (126992 hora del sistema, 129029 posicion GNSS): true una vez por cada hora nueva
    bool takeUtc(int64_t& unixSec) { if (!utcNew_) return false; utcNew_ = false; unixSec = utc_; return true; }

private:
    N2kFastPacket fast_;
    AisReport ais_;
    N2kStats stats_;
    int64_t utc_ = 0;
    bool utcNew_ = false;
};

}  // namespace plotter
#endif

/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Utilidades comunes a todas las entradas (serie, TCP, UDP): troceo en lineas y filtro de tramas repetidas.
#ifndef PLOTTER_STREAM_H_
#define PLOTTER_STREAM_H_

#include <cstddef>
#include <cstdint>

namespace plotter {

// Junta bytes hasta completar una linea. Un datagrama UDP puede traer varias lineas; un flujo TCP o serie, trozos sueltos.
class LineBuffer {
   public:
    // true cuando "c" completa una linea no vacia: line() la devuelve sin CR/LF hasta la siguiente llamada a push()
    bool push(char c);
    const char* line() const { return buf_; }
    uint32_t dropped() const { return dropped_; }       // lineas descartadas por largas

   private:
    static constexpr size_t kMax = 128;                 // NMEA 0183 no pasa de 82 caracteres
    char buf_[kMax];
    size_t len_ = 0;
    bool overflow_ = false, done_ = false;
    uint32_t dropped_ = 0;
};

// Recuerda las ultimas tramas para no procesar ni reenviar dos veces la misma (dos receptores, o serie y red a la vez)
class DupFilter {
   public:
    explicit DupFilter(uint32_t windowMs = 3000) : windowMs_(windowMs) {}
    bool seen(const char* line, uint32_t nowMs);        // true si es repetida dentro de la ventana; si no, la apunta

   private:
    static constexpr int kSlots = 64;
    uint32_t hash_[kSlots] = {};
    uint32_t time_[kSlots] = {};
    bool used_[kSlots] = {};
    int next_ = 0;
    uint32_t windowMs_;
};

}  // namespace plotter
#endif

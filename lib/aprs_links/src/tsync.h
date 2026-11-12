/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Reloj interno de la placa (hora UTC del sistema): lo pone el GPS (sentencias RMC / ZDA de cualquier fuente, NMEA 2000, el
 * GNSS del tracker) y, sin GPS, NTP (ntp.roa.es del Real Instituto y Observatorio de la Armada, hora oficial de Espana; de
 * reserva hora.roa.es y pool.ntp.org). La hora de otro equipo APRS LoRa+ (el plotter al instrumento de viento) cuenta despues.
 * Las reglas estan en el nucleo (plotter_core/timesync.h, ClockSync). Lo usan el plotter, el instrumento, la Display y el rotor.
 */
#pragma once
#include <Arduino.h>

#include "timesync.h"

namespace tsync {

struct Status {
    plotter::TimeSource source = plotter::TimeSource::None;   // quien puso la hora (None: nadie en las ultimas 6 h)
    uint32_t ageS = 0;                                        // desde que se puso
    uint32_t sets = 0;
    bool ntp = false;                                         // cliente NTP en marcha
    char ntpServer[40] = "";
};

// Arranca el cliente NTP del sistema (con la red ya en marcha). server vacio o nullptr = ntp.roa.es. Se puede repetir.
void startNtp(const char* server = nullptr);
// Hora de una fuente: pone el reloj si toca (ClockSync). ms: milisegundos de esa hora.
bool offer(plotter::TimeSource s, int64_t unixSec, int ms = 0);
// Una sentencia NMEA 0183: RMC o ZDA con hora (las demas no hacen nada)
bool offerNmea(const char* line, plotter::TimeSource s = plotter::TimeSource::Gps);
int64_t now();                                                // segundos UNIX, 0 si el reloj aun no tiene hora
Status status();
}

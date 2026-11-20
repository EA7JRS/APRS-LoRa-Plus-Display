/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Mensajes de radiotelefonia de socorro, urgencia y seguridad para leer en voz por el canal 16 (o 2182 kHz): MAYDAY,
// MAYDAY RELAY, PAN PAN y SECURITE, en espanol o en ingles, con los datos de la embarcacion, la situacion y la hora.
// Sigue el orden de los procedimientos del SMSSM (llamada, identificacion, situacion, naturaleza, auxilio, personas,
// otros datos); redaccion propia. Las naturalezas de socorro van en el orden de las de la LSD (100 a 110), asi una
// alerta LSD recibida se puede retransmitir con su naturaleza. Sin Arduino.
#pragma once
#include <cstdint>
#include <string>

namespace plotter {

enum class RtKind { Mayday = 0, MaydayRelay = 1, PanPan = 2, Securite = 3 };
enum class RtLang { Es = 0, En = 1 };

struct RtInput {
    RtKind kind = RtKind::Mayday;
    RtLang lang = RtLang::Es;
    std::string name;            // nombre de la embarcacion
    std::string callsign;        // distintivo de llamada maritimo
    std::string mmsi;            // 9 cifras
    bool hasPos = false;
    double lat = 0, lon = 0;
    int utcMin = -1;             // hora UTC de la situacion en minutos del dia; -1 sin hora
    int nature = 0;              // indice de rtNatureName para ese tipo de mensaje
    int persons = -1;            // personas a bordo; -1 = no se dice
    std::string description;     // tipo, eslora, color...; vacio = no se dice
    std::string extra;           // otros datos (o el texto del aviso de seguridad)
    // MAYDAY RELAY: el buque en peligro
    std::string relayName, relayMmsi;
    bool relayHasPos = false;
    double relayLat = 0, relayLon = 0;
    int relayUtcMin = -1;
    int relayPersons = -1;
};

int rtNatureCount(RtKind k);
const char* rtNatureName(RtKind k, int i, RtLang l);
// Naturaleza de socorro LSD (100..110) -> indice de MAYDAY; -1 si no es una de ellas
int rtNatureFromDsc(int dscNature);

std::string rtPhonetic(const std::string& callsign, RtLang l);     // "ECHO ALFA SEIS ..." (las cifras en el idioma)
std::string rtDigits(const std::string& number, RtLang l);         // MMSI cifra a cifra, en palabras
std::string rtPosition(double lat, double lon, RtLang l);          // "36 grados 31,8 minutos Norte, 006 grados ..."

// El mensaje entero, una linea por frase, en mayusculas, para leerlo tal cual
std::string radiotelMessage(const RtInput& in);

}  // namespace plotter

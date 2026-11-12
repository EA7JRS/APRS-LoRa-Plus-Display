/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Avisos meteorologicos oficiales (el plan del plotter: AEMET en Espana, Meteoalarm en el resto de Europa; niveles amarillo,
// naranja y rojo). Lee mensajes CAP (los <alert> de AEMET OpenData, con su poligono) y las entradas del canal Atom de
// Meteoalarm (<entry> con etiquetas cap:, sin poligono), los corta de un flujo de bytes tal como llega de la red y dice
// cuales tocan a una posicion. C++ puro, probado en el PC (test/test_extra.cpp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wxw {

enum Level { NONE = 0, YELLOW = 1, ORANGE = 2, RED = 3 };

struct Warning {
    std::string id, event, area, desc, instr, prob, phen, source;   // phen: "Lluvias", "Costeros"...; source: "AEMET", "Meteoalarm"
    std::string onset, expires;             // como llegan (ISO 8601 con su desfase)
    int64_t onsetS = 0, expiresS = 0;       // segundos UTC desde 1970; 0 = no se sabe
    int level = NONE;
    bool hasArea = false;                   // traia poligono
    bool inside = false;                    // la posicion dada cae dentro
    float distKm = -1;                      // al punto mas cercano del poligono (0 si esta dentro); -1 sin poligono o sin posicion
};

int64_t isoToEpoch(const std::string& iso);                 // "2026-09-10T00:00:00+02:00" -> segundos UTC; 0 si no vale
std::string shortTime(const std::string& iso);              // "10 00:00" (dia y hora local del aviso)
const char* levelName(int level);                           // "yellow", "orange", "red"
uint32_t levelColor(int level);                             // colores de Meteoalarm
// Un <alert> CAP o una <entry> de Meteoalarm. false si no es un aviso en vigor que interese (nivel verde, cancelado, prueba).
bool parse(const std::string& xml, Warning& w, bool havePos, double lat, double lon);
// Texto del area contiene alguna de las palabras de "filter" (separadas por comas; sin distinguir mayusculas ni acentos basicos)
bool areaMatches(const std::string& area, const std::string& filter);
bool expired(const Warning& w, int64_t nowS);
// Orden de la lista: dentro de la zona antes, mas nivel antes, lo que empieza antes
bool before(const Warning& a, const Warning& b);

// Corta los avisos de un flujo (un tar de AEMET o el Atom de Meteoalarm): se le dan los bytes segun llegan.
class Scanner {
   public:
    // true cuando "out" trae un bloque completo; se vuelve a llamar con n = 0 hasta que devuelva false
    bool feed(const uint8_t* data, size_t n, std::string& out);
    bool gzip() const { return gzip_; }     // el flujo venia comprimido: no se puede leer
    size_t blocks() const { return blocks_; }
   private:
    std::string buf_;
    bool in_ = false, gzip_ = false, first_ = true;
    int kind_ = 0;                          // 1 <alert, 2 <entry
    size_t blocks_ = 0;
    static constexpr size_t MAX = 24000;    // un aviso mas largo se corta (el poligono va al final)
};

} // namespace wxw

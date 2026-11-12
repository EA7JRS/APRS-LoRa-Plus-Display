/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Ayudas a la navegacion: alcance geografico y luminoso de una luz, ritmos de las luces (abreviaturas internacionales y
// espanolas), marcas del sistema de balizamiento IALA y radio de borneo en el fondeo. Formulas y reglas del manual NAVGUIDE
// de la IALA; contrastar con la edicion vigente antes de fiarse de un detalle.
#ifndef PLOTTER_LIGHTS_H_
#define PLOTTER_LIGHTS_H_

#include <cstdint>

namespace plotter {

// Distancia a la que asoma una luz por el horizonte: 2,03 x (raiz(altura del ojo) + raiz(altura de la luz)), en metros -> millas
double geographicRangeNm(double eyeHeightM, double lightHeightM);
// Alcance con la visibilidad real a partir del alcance nominal de la carta (que supone 10 millas de visibilidad). Ley de Allard.
double luminousRangeNm(double nominalRangeNm, double visibilityNm);
// Lo que se vera de verdad: el menor de los dos
double sightingRangeNm(double nominalRangeNm, double visibilityNm, double eyeHeightM, double lightHeightM);

// Fondeo: radio del circulo de borneo con "rodeM" de cadena o cabo filado, en "depthM" de agua y la roldana a "bowHeightM"
// sobre el agua. 0 si no hay cadena para llegar al fondo.
double swingRadiusM(double rodeM, double depthM, double bowHeightM = 0);
// Relacion cadena / fondo (4 = cuatro veces el fondo)
double scopeRatio(double rodeM, double depthM, double bowHeightM = 0);

enum class LightClass : uint8_t { Unknown, Fixed, Occulting, Isophase, Flashing, LongFlash, Quick, VeryQuick, UltraQuick, Morse, Alternating };
enum class LightColor : uint8_t { White, Red, Green, Yellow, Blue, Unknown };

struct LightCharacter {
    LightClass cls = LightClass::Unknown;
    uint8_t group = 1;              // destellos u ocultaciones del grupo
    uint8_t group2 = 0;             // segundo grupo de los compuestos: Fl(2+1)
    bool longFlashAfter = false;    // Q(6)+LFl
    LightColor color = LightColor::White;
    char morse = 0;                 // Mo(A)
    double periodS = 0;             // 0 = sin dato
};

// "Fl(2) W 10s", "Q(6)+LFl 15s", "VQ(3) 5s", "Iso R 4s", "Oc(2) G 8s", "Mo(A) 6s", "Fl(2+1) R 12s", "F R"; y en espanol
// "GpD(2) B 10s", "DL 10s", "Ct", "Rp(3) 5s", "GpOc(2) V 8s". false si no se entiende.
bool parseLightCharacter(const char* text, LightCharacter& out);
// Luz encendida o apagada en el instante "tMs" (para animarla en la carta)
bool lightIsOn(const LightCharacter& c, uint32_t tMs);

// Lista de luces propia del plotter (microSD): una luz por linea, campos separados por ';'
//   nombre;latitud;longitud;caracteristica;altura de la luz en m;alcance nominal en mn
// Ejemplo: "Faro de ejemplo;36.5297;-6.3147;Fl(2) W 10s;49;20". Las lineas vacias o que empiezan por '#' se saltan.
struct LightRecord {
    char name[40] = "";
    double lat = 0, lon = 0;
    LightCharacter character;
    char characterText[24] = "";    // tal como venia
    double heightM = 0;             // 0 = sin dato
    double nominalNm = 0;           // 0 = sin dato
};
bool parseLightRecord(const char* line, LightRecord& out);
// Distancia a la que se vera esa luz con esta visibilidad y altura del ojo; 0 si faltan el alcance o la altura.
double lightSightingRangeNm(const LightRecord& l, double visibilityNm, double eyeHeightM);

// Luces de OpenStreetMap / OpenSeaMap (licencia ODbL): cada luz lleva las etiquetas "seamark:light:character" ("Fl",
// "Q+LFl", "Mo"...), "seamark:light:group" ("2", "2+1", "A"), "seamark:light:period" (segundos), "seamark:light:colour"
// ("white", "red"...), "seamark:light:height" (m) y "seamark:light:range" (mn). Cualquiera puede faltar (nullptr o "").
bool lightCharacterFromSeamark(const char* character, const char* group, const char* period, const char* colour, LightCharacter& out,
                               char* textOut = nullptr, int textSize = 0);
bool lightRecordFromSeamark(const char* name, double lat, double lon, const char* character, const char* group, const char* period,
                            const char* colour, const char* height, const char* range, LightRecord& out);
// Consulta a la API Overpass de las luces de un recuadro (sur, oeste, norte, este), respuesta JSON. Devuelve la longitud
// de la direccion, 0 si no cabe.
constexpr const char* kOverpassUrl = "https://overpass-api.de/api/interpreter";
int overpassLightsUrl(char* out, int size, double south, double west, double north, double east);
// Linea de la lista de luces de la microSD a partir de un registro (para guardar lo descargado)
int formatLightRecord(char* out, int size, const LightRecord& l);

// Marcas del sistema de balizamiento IALA
enum class IalaMark : uint8_t {
    Unknown, PortHand, StarboardHand, PreferredChannelPort, PreferredChannelStarboard, CardinalN, CardinalE, CardinalS, CardinalW,
    IsolatedDanger, SafeWater, Special, EmergencyWreck, Light, LeadingLight, Racon, Structure
};
const char* ialaMarkNameEs(IalaMark m);
// Tipo de ayuda del mensaje AIS 21 (0-31) -> marca; "floating" dice si es boya o baliza fija
IalaMark ialaMarkFromAisAton(int atonType, bool* floating = nullptr);
// "Que luz veo": marca mas probable por el ritmo y el color (region A: babor rojo, estribor verde)
IalaMark ialaMarkFromLight(const LightCharacter& c, bool regionB = false);

}  // namespace plotter
#endif

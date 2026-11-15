/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Datos de la embarcacion propia y su sincronizacion con el transpondedor APRS LoRa+ MAPRS (o cualquier tracker con el perfil
// nautico AIS) por la orden y el evento "vessel" de su servicio de control BLE (docs/BLE_CONTROL.md del tracker).
// Regla: el plotter manda. Si el plotter no tiene datos, toma los del transpondedor; si los tiene y son distintos, los envia, y
// despues se queda con lo que el transpondedor confirma (el ajusta el nombre a lo que admite AIS, la eslora de clase B, el SSID...).
#ifndef PLOTTER_VESSEL_H_
#define PLOTTER_VESSEL_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace plotter {
namespace vessel {

struct Data {
    char     name[21] = "";         // nombre (AIS: 20 caracteres)
    uint32_t mmsi     = 0;          // 0 = sin MMSI
    char     call[10] = "";         // indicativo APRS base (el transpondedor le pone el SSID del tipo de barco)
    char     mcall[8] = "";         // distintivo de llamada maritimo del MMSI (vacio = el indicativo APRS)
    int      mcok     = 0;          // distintivo comprobado: 0 no, 1 MarineTraffic, 2 MARS de la UIT (mtcheck)
    int      type     = 0;          // tipo del perfil nautico AIS (0 velero de recreo ... 16 otro buque)
    int      lengthM  = 0;          // eslora, m
    int      beamM    = 0;          // manga, m
    int      draftDm  = 0;          // calado, decimas de metro
};

int         typeCount();                    // 17
const char* typeName(int type);             // en espanol
char        typeClass(int type);            // 'A', 'S' (clase B "SO") o 'C' (clase B "CS")
bool        classB(int type);
bool        configured(const Data& d);      // tiene nombre o MMSI
bool        same(const Data& a, const Data& b);     // indicativo sin SSID, nombre sin distinguir mayusculas
bool        validMmsi(uint32_t mmsi);       // 200000000 - 799999999 (como el tracker)

const char* queryCommand();                 // {"c":"vessel"}
// Orden para cambiar los datos. classBOnly: el transpondedor solo admite clase B (no se manda un tipo de clase A)
size_t      setCommand(const Data& d, bool classBOnly, char* out, size_t size);
// Evento {"e":"vessel",...} -> datos. false si no es ese evento o el tracker no tiene perfil nautico ("ok":false)
bool        parseEvent(const std::string& line, Data& out, bool& classBOnly);

enum Action { NONE = 0, QUERY, PUSH, ADOPT };
enum State  { NO_LINK = 0, WAITING, SYNCED, SENT, ADJUSTED, NO_PROFILE, NOT_LICENSED };

class Sync {
public:
    Action link(bool up);                   // enlace con un tracker que admite ordenes: QUERY al subir
    Action remote(const Data& r, bool classBOnly, const Data& local);   // evento recibido: PUSH, ADOPT (copiar r) o NONE
    Action noProfile();                     // el tracker no tiene perfil nautico AIS
    Action localChanged(const Data& local); // el usuario ha guardado los datos: PUSH si hay enlace y son otros
    void   blocked();                       // PUSH sin indicativo validado: no se manda nada (NOT_LICENSED)
    State  state() const { return state_; }
    bool   classBOnly() const { return classB_; }
    const Data& last() const { return last_; }  // lo ultimo que dijo el transpondedor
private:
    State state_  = NO_LINK;
    bool  up_     = false;
    bool  pushed_ = false;
    bool  known_  = false;
    bool  classB_ = false;
    Data  last_;
};

}  // namespace vessel
}  // namespace plotter
#endif

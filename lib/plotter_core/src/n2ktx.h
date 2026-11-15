/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Emision NMEA 2000 para los accesorios (instrumento de viento): identificador CAN, PGN 130306 de viento, NAME del equipo
// y reclamacion de direccion (ISO 11783-5: PGN 60928, respuesta a la peticion ISO 59904 y cambio de direccion si otro
// equipo con mejor NAME reclama la misma). Formatos contrastados con canboat; no es un nodo certificado.
#ifndef PLOTTER_N2KTX_H_
#define PLOTTER_N2KTX_H_

#include <stdint.h>

namespace plotter {

// Identificador de 29 bits. Con PGN dirigido (PF < 240) el byte bajo del PGN debe ser 0 y "dst" va en su lugar.
uint32_t n2kMakeId(uint8_t prio, uint32_t pgn, uint8_t src, uint8_t dst = 255);

// PGN 130306, una trama de 8 bytes. Velocidad en nudos; angulo en grados (aparente: desde la proa, negativo por babor;
// real con ref 0: direccion de la que viene, respecto al norte verdadero). ref: 0 norte verdadero, 2 aparente,
// 3 real respecto al barco. Angulo NAN = sin dato.
void n2kWind(uint8_t sid, double knots, double angleDeg, uint8_t ref, uint8_t out[8]);

// PGN 129283 "Cross Track Error", una trama de 8 bytes. xteM en metros (positivo = el barco a estribor del tramo, como
// RouteStatus::xteNm). terminated = navegacion terminada. Modo 0 (autonomo). NAN = sin dato.
void n2kXte(uint8_t sid, double xteM, bool terminated, uint8_t out[8]);

// PGN 129284 "Navigation Data", fast packet de 34 bytes (out[34]). Demoras en radianes verdaderos; distancias en metros;
// posicion del destino en grados; velocidad de acercamiento en m/s. calcGreatCircle: false = loxodromica. etaDays/etaSecs
// = fecha/hora estimada de llegada (0 = sin dato). NAN en un double = sin dato.
struct N2kNav {
    double distanceM = 0.0 / 0.0;      // distancia al waypoint de destino
    bool   magnetic = false;           // demoras magneticas (true) o verdaderas (false)
    bool   perpCrossed = false;        // perpendicular al destino cruzada
    bool   arrivalEntered = false;     // dentro del circulo de llegada
    bool   calcGreatCircle = true;     // true ortodromica, false loxodromica
    double bearingOrigToDestRad = 0.0 / 0.0;
    double bearingPosToDestRad = 0.0 / 0.0;
    uint32_t originWp = 0xFFFFFFFF, destWp = 0xFFFFFFFF;
    double destLat = 0.0 / 0.0, destLon = 0.0 / 0.0;
    double closingVelMs = 0.0 / 0.0;
    uint16_t etaDays = 0xFFFF;
    double   etaSecs = 0.0 / 0.0;
};
void n2kNavData(uint8_t sid, const N2kNav& nav, uint8_t out[34]);

// Parte un payload largo en tramas de fast packet (ISO 11783-3). seq: contador 0..7 que cambia en cada mensaje nuevo.
// Escribe las tramas de 8 bytes seguidas en "out" (cada una 8 bytes) y devuelve cuantas. frames necesarias: 1 + ceil((len-6)/7).
int n2kFastFrames(const uint8_t* payload, int len, uint8_t seq, uint8_t* out, int outCap);

struct N2kName {
    uint32_t unique = 0;            // 21 bits: numero de serie (en la placa, de la MAC)
    uint16_t manufacturer = 2046;   // 11 bits: codigo sin registrar (no es de ningun fabricante)
    uint8_t deviceInstance = 0, deviceFunction = 130, deviceClass = 85;   // 85 / 130: entorno exterior, atmosferico
    uint8_t systemInstance = 0, industryGroup = 4;                         // 4: marina
    bool arbitraryAddress = true;
};
uint64_t n2kNameValue(const N2kName& n);

class N2kAddressClaim {
   public:
    struct Frame { uint32_t id = 0; uint8_t d[8] = {}; int len = 0; };
    N2kAddressClaim(uint64_t name, uint8_t preferred);
    // Arranca o vuelve a empezar: deja en "out" la reclamacion que hay que mandar
    void start(uint32_t nowMs, Frame& out);
    // Trama recibida del bus. true si hay que mandar "out" (reclamacion propia, nueva o repetida)
    bool onFrame(uint32_t canId, const uint8_t* d, int len, uint32_t nowMs, Frame& out);
    // Se puede emitir: 250 ms tras la ultima reclamacion sin que nadie la dispute, y con direccion
    bool ready(uint32_t nowMs) const;
    uint8_t address() const { return addr_; }
    bool lost() const { return addr_ == 254; }      // sin direccion libre: no se emite nada
    uint32_t changes() const { return changes_; }

   private:
    void claimFrame(Frame& out) const;
    bool pickNext();
    uint64_t name_;
    uint8_t addr_;
    uint32_t claimedMs_ = 0, changes_ = 0;
    bool started_ = false;
    uint8_t taken_[32] = {};        // direcciones 0..251 reclamadas por otros equipos
};

}  // namespace plotter
#endif

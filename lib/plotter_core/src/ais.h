/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Decodificador AIS (ITU-R M.1371) a partir de tramas VDM / VDO: buques (1, 2, 3, 5, 18, 19, 24, 27), estaciones base (4),
// aeronaves SAR (9), mensajes de seguridad (12, 14) y ayudas a la navegacion (21).
// La comprobacion de posiciones y las marcas de calidad siguen la idea de AIS-catcher (jvde-github, GPL-3.0).
#ifndef PLOTTER_AIS_H_
#define PLOTTER_AIS_H_

#include <cstdint>

namespace plotter {

enum class AisKind : uint8_t { Vessel, BaseStation, SarAircraft, Aton, Safety };

struct AisReport {
    uint32_t mmsi = 0;
    int type = 0;
    AisKind kind = AisKind::Vessel;
    bool oversized = false;         // mas bits de los que fija la norma para ese mensaje (se acepta, se marca)
    bool own = false;               // llego en VDO: es el barco propio
    bool hasPos = false;
    double lat = 0, lon = 0;
    bool hasSog = false, hasCog = false, hasHeading = false;
    double sogKn = 0, cogDeg = 0;
    int heading = 0;
    int navStatus = -1;             // -1 sin dato
    bool hasStatic = false;
    char name[35] = "";             // 20 caracteres; las ayudas a la navegacion pueden llevar 14 mas
    char callsign[8] = "";
    int shipType = -1;
    bool hasDim = false;            // metros desde la antena a proa, popa, babor y estribor
    int toBow = 0, toStern = 0, toPort = 0, toStarboard = 0;
    bool hasVoyage = false;         // mensaje 5
    double draughtM = 0;
    char destination[21] = "";
    bool hasAlt = false;            // mensaje 9
    int altM = 0;
    int atonType = -1;              // mensaje 21: tipo de ayuda (1-31)
    bool atonVirtual = false, atonOffPosition = false;
    uint32_t destMmsi = 0;          // mensaje 12: destinatario
    char text[162] = "";            // mensajes 12 y 14
};

enum class AisError : uint8_t { None, Pending, Fragment, BadChar, Undersized, Unsupported, BadMmsi };

// Posicion nueva frente a la anterior del mismo MMSI: descarta saltos imposibles (trama danada, MMSI equivocado o repetido).
enum class AisPosVerdict : uint8_t { Unknown, Valid, Jump };
AisPosVerdict aisJudgePosition(bool known, double lat0, double lon0, bool wasStopped, long seconds, double lat, double lon,
                               bool aircraft);

class AisDecoder {
   public:
    // Campos de la trama: total de fragmentos, numero de fragmento, identificador de secuencia (-1 si vacio), carga y bits de relleno.
    // true cuando hay un mensaje completo y reconocido en "out".
    bool feed(int total, int num, int seqId, const char* payload, int fillBits, bool own, AisReport& out);
    AisError lastError() const { return err_; }     // por que la ultima llamada devolvio false (Pending = falta un fragmento)

   private:
    bool decode(const uint8_t* six, int nbits, bool own, AisReport& out);
    static constexpr int kMaxChars = 180;       // tres fragmentos largos
    uint8_t buf_[kMaxChars + 8];
    int len_ = 0, seq_ = -2, next_ = 1, total_ = 0;
    AisError err_ = AisError::None;
};

// Codificador (para el simulador y las pruebas): arma la trama "!AIVDM,1,1,,A,<carga>,0*XX" de un informe de posicion de
// clase A (mensaje 1) o del nombre de un clase B (mensaje 24, parte A). Devuelve la longitud, 0 si no cabe.
int aisEncodePosition(char* out, int size, uint32_t mmsi, double lat, double lon, double sogKn, double cogDeg, int heading,
                      int navStatus = 0);
int aisEncodeName(char* out, int size, uint32_t mmsi, const char* name);

}  // namespace plotter
#endif

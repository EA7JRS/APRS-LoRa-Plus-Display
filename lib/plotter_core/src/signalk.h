/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Signal K: rutas y unidades (SI: m/s, radianes, kelvin, pascales) frente a la tabla de datos del plotter, suscripcion,
// avisos y envio de valores propios. El JSON lo lee el firmware (ArduinoJson) y pasa aqui cada ruta con su valor; los
// objetos se aplanan: "navigation.position.latitude", "navigation.attitude.roll".
#ifndef PLOTTER_SIGNALK_H_
#define PLOTTER_SIGNALK_H_

#include <cstdint>

#include "store.h"

namespace plotter {

constexpr uint16_t kSignalKTcpPort = 8375;          // deltas JSON, uno por linea
constexpr uint16_t kSignalKNmeaPort = 10110;        // NMEA 0183 del servidor
// Servicios mDNS con los que se anuncia el servidor
constexpr const char* kSignalKMdnsTcp = "_signalk-tcp";
constexpr const char* kSignalKMdnsWs = "_signalk-ws";

// Aplica un valor del barco propio a la tabla. false si la ruta no se usa.
bool skApply(const char* path, double siValue, Source src, uint32_t nowMs, DataStore& store);

// Linea que pide todos los datos del barco propio cada "periodMs". Devuelve la longitud (con "\r\n"), 0 si no cabe.
int skSubscribe(char* out, int size, uint32_t periodMs = 1000);

// Avisos: "notifications.mob" -> "mob"; nullptr si la ruta no es un aviso
const char* skNotificationKey(const char* path);
// Titulo en espanol de los avisos habituales; nullptr si no se conoce (se muestra el mensaje del servidor)
const char* skNotificationTitleEs(const char* key);

// Plotter como sensor: ruta y valor SI de un dato de la tabla (nullptr si ese dato no se publica)
const char* skPathFor(Key k);
double skToSi(Key k, double value);
// Delta con un valor. Devuelve la longitud (con "\r\n"), 0 si no cabe.
int skDelta(char* out, int size, const char* path, double siValue);

}  // namespace plotter
#endif

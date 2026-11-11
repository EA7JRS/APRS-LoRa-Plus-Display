/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Red de la 7B v2: WiFi, caja del rotor (TCP o BLE), DX Cluster (telnet), tiempo de internet (OpenWeatherMap) y firmware
// del tracker por internet (se baja aqui y se manda por BLE). Cada enlace vive en su tarea; la interfaz solo llama a esto.
#pragma once
#include <cstdint>
#include <string>

namespace net {

void begin();                       // tareas y WiFi con los ajustes guardados
void loop();                        // desde loop(): reparte lo que llega (rotor, DX) y lo periodico (tiempo)

// WiFi
struct Wifi { bool configured = false, connected = false; std::string ssid, ip; int rssi = 0; };
Wifi wifi();
void wifiApply();                   // tras cambiar el SSID o la clave en los ajustes
bool timeValid();                   // hora de internet (NTP) puesta

// Caja del rotor
enum class Link { Off, Connecting, Linked, Failed };
Link rotorState();
const char* rotorStateText();
void rotorApply();                  // tras cambiar el tipo de enlace, la direccion o el PIN
void rotorSend(const std::string& line);
bool rotorPoll(std::string& line);

// DX Cluster
Link dxState();
const char* dxStateText();
void dxConnect();                   // con dxHost / dxPort / dxCall de los ajustes
void dxDisconnect();
void dxSend(const std::string& line);   // orden al nodo (sin \n)
bool dxPoll(std::string& line);

// Tiempo de internet
std::string wxStatus();             // "OWM Sevilla 24.1 C, 3 min ago" o el motivo
void wxNow();                       // bajarlo ya

// Firmware del tracker
struct Fw {
    int phase = 0;                  // 0 nada, 1 buscando, 2 hay version, 3 bajando, 4 enviando, 5 hecho, 6 error
    std::string latest, text;
    uint32_t done = 0, total = 0;
};
Fw fw();
void fwCheck(const std::string& board);
void fwInstall(const std::string& board);
void fwCancel();

} // namespace net

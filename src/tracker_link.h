/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Enlace BLE con el tracker APRS LoRa+: cliente del servicio de control (JSON por lineas, docs/BLE_CONTROL.md del firmware),
 * con emparejamiento por PIN. Toda la pila BLE vive en su propia tarea; la interfaz solo llama a send() y poll().
 */
#pragma once
#include <string>

namespace trk {

enum class State { Idle, Scanning, Connecting, Pairing, Linked, Failed };

void begin();                                   // arranca la tarea
State state();
const char* stateText();
std::string peerName();                         // nombre del tracker al que se esta conectado
void send(const std::string& line);             // pone una orden en cola (termina en \n)
bool poll(std::string& line);                   // saca una linea de evento recibida; false si no hay
void setPin(uint32_t pin);                      // PIN de 6 cifras del tracker (WebUI > Bluetooth > Control PIN)
uint32_t pin();
void setPeerAddress(const std::string& addr);   // vacio = el primer tracker que se oiga
std::string peerAddress();
void forgetPairing();                           // olvida los emparejamientos (hay que repetirlo tras cambiar el PIN del tracker)
void reconnect();                               // corta y vuelve a buscar
// Firmware por BLE (caracteristica ...0005, escrituras sin respuesta): trozos de <= 240 bytes; false si la cola esta llena
bool fwPush(const uint8_t* data, size_t len);
size_t fwQueued();                              // trozos que faltan por escribir
void fwClear();
// El escaneo BLE lo comparten el enlace con el tracker y el de la caja del rotor (net.cpp)
void scanLock();
void scanUnlock();
}

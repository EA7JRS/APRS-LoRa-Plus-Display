/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Enlace BLE con la pasarela (XIAO): el plotter es el central, busca un equipo que anuncie el servicio de datos NMEA,
 * se conecta y recibe lineas NMEA 0183 por notificaciones. Protocolo en docs/BLE_NMEA.md. Va en una tarea aparte.
 * Tambien se conecta a un tracker APRS LoRa+: se empareja con su PIN y se suscribe a la caracteristica NMEA de su
 * servicio de control (docs/BLE_CONTROL.md del firmware del tracker).
 */
#pragma once
#include <Arduino.h>

namespace ble {

enum State { OFF = 0, SCANNING = 1, CONNECTING = 2, CONNECTED = 3 };

struct Status {
    int state = OFF;
    char name[24] = "";                 // nombre del equipo conectado (o del ultimo)
    char addr[18] = "";                 // su direccion, "aa:bb:cc:dd:ee:ff"
    int rssi = 0;
    uint32_t bytes = 0;                 // recibidos
    uint32_t connects = 0;              // conexiones logradas
    bool tracker = false;               // el equipo es un tracker APRS LoRa+ (servicio de control, con PIN)
    bool pinFailed = false;             // el tracker no ha aceptado el PIN
    bool canSend = false;               // el equipo tiene la caracteristica de ordenes (...0002): el instrumento de viento
    uint32_t sent = 0;                  // bytes mandados al equipo
};

// on = false: desconecta y deja de buscar. addr: direccion del equipo ya conocido ("" = el primero que aparezca).
// pin: el PIN de control del tracker (WebUI del tracker > Bluetooth > Control PIN); la pasarela no lo usa.
void set(bool on, const char* addr, uint32_t pin);
const Status& status();
int read(uint8_t* buf, int max);        // bytes recibidos desde la ultima llamada
// Texto para el equipo (NMEA 0183): rumbo y velocidad al instrumento de viento. false si no hay enlace o no cabe.
bool write(const char* text, int len);
}

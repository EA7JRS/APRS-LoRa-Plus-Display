/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Signal K por TCP (puerto 8375): cada linea es un mensaje JSON. Aqui se lee con ArduinoJson y se pasa a la tabla de
 * datos con el nucleo (signalk). Solo el barco propio; los avisos (notifications.*) se devuelven para la ventana de alerta.
 */
#pragma once
#include <Arduino.h>

#include "store.h"

namespace sk {

struct Notice {
    bool present = false;
    char key[48] = "";          // "mob", "navigation.anchor"...
    char state[12] = "";        // nominal, normal, alert, warn, alarm, emergency
    char message[96] = "";
};

// Procesa una linea. Devuelve cuantos valores han entrado en la tabla; si trae un aviso, lo deja en "notice".
int feedLine(const char* json, uint32_t nowMs, plotter::DataStore& store, Notice& notice);
}

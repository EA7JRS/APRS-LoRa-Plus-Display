/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * CAT del reloj: cliente del protocolo de texto de rigctld (Hamlib, TCP, puerto 4532 de serie). Hamlib no cabe en el ESP32:
 * rigctld va en un PC o una Raspberry Pi con la radio y la pantalla le habla por WiFi, como el rotor con rotctld.
 * C++ puro sobre un transporte cualquiera (WiFiClient en la placa, un socket en el PC: test/test_rig.cpp).
 */
#pragma once
#include <cstddef>

namespace hcrig {

struct Io {
    bool (*send)(void* ctx, const char* text);
    bool (*line)(void* ctx, char* buf, size_t n, int timeoutMs);     // una linea, sin el fin de linea
    void* ctx;
};

// "host" o "host:puerto" (4532 si no se dice). false si esta vacio o mal.
bool parseAddress(const char* text, char* host, size_t n, int& port);

bool getFreq(Io& io, double& hz);
bool getMode(Io& io, char mode[8]);
// 0 si la radio lo acepta; si no, el codigo RPRT de Hamlib (negativo) o -1000 si no hay respuesta
int setFreq(Io& io, double hz);
int setMode(Io& io, const char* mode);

// modo de Hamlib para el modo de un spot en esa frecuencia: "SSB" es LSB por debajo de 10 MHz (menos en 60 m) y USB por
// encima; FT8, FT4 y demas digitales, PKTUSB; "" = no tocar el modo
const char* modeFor(const char* spotMode, double mhz);
// sintoniza: frecuencia y despues modo (si la radio no tiene PKTUSB se queda en USB). true si la frecuencia ha entrado.
bool tune(Io& io, double mhz, const char* spotMode);

}

/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Red del reloj: WiFi, hora por NTP y una tarea que va pidiendo los datos a la API del servidor OpenHamClock.
 */
#pragma once
#include "hc_model.h"

namespace hcnet {
void begin(hc::Model* model, const hc::Config* cfg);
// el modelo lo escribe la tarea de red: quien lo lea (la interfaz) lo hace entre lock() y unlock()
// CAT: pide a la tarea de la radio que sintonice (MHz y el modo del spot; "" = no tocar el modo)
void tune(double mhz, const char* spotMode);
void lock();
void unlock();
}

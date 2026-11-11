/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz LVGL 9.6 del reloj (1024x600, Waveshare ESP32-S3-Touch-LCD-7B). No depende de Arduino: se dibuja igual en el PC
 * (tools/hamclock_sim).
 */
#pragma once
#include "hc_model.h"

namespace hcui {
// onSave: se ha pulsado Save en los ajustes (el que llama los guarda y reinicia)
// onTune: se ha pulsado TUNE con un spot o una activacion elegidos (MHz y su modo); solo si hay CAT en los ajustes
void begin(hc::Model* model, const hc::Config* cfg, void (*onSave)(const hc::Config&), void (*onTune)(double mhz, const char* mode));
// repinta lo que haya cambiado (unas veces por segundo). El que llama protege el modelo mientras tanto.
void update();
void openSettings();
}

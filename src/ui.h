/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "protocol.h"

namespace ui {
enum { THEME_APP = 0, THEME_ROTOR = 1 };   // paleta de la app Android (por defecto) o de la pantalla del rotor
// onThemeChange: se llama al elegir otra paleta en Settings (main la guarda y reinicia)
void begin(proto::Model* model, int theme, void (*onThemeChange)(int));
// Avisos oficiales de la WiFi propia (alerts_net), junto a los del tracker
void setNetAlerts(std::vector<plotter::Alert>& v, const plotter::SpaceWx& space);
void update();                     // repinta lo que haya cambiado en el modelo (llamar unas veces por segundo)
bool mirrorVisible();              // la pestaña espejo esta abierta (hay que pedir 'screen')
bool heardVisible();
bool messagesVisible();
}

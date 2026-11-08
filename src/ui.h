/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "protocol.h"

namespace ui {
void begin(proto::Model* model);
void update();                     // repinta lo que haya cambiado en el modelo (llamar unas veces por segundo)
bool mirrorVisible();              // la pestaña espejo esta abierta (hay que pedir 'screen')
bool heardVisible();
bool messagesVisible();
}

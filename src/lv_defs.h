// APRS LoRa+ Display - Copyright (C) 2026 Manuel - EA7JRS - GPL-3.0-or-later
// Ajustes de LVGL 9.6 (se incluye en todo con -include; no hay lv_conf.h). Lo que lleva '&' no se puede pasar con -D.
#pragma once
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565      // RGB565 como el panel (sustituye a LV_COLOR_DEPTH 16)
#define LV_FONT_DEFAULT (&lv_font_montserrat_20)
// malloc/memcpy/sprintf de la libreria C: con PSRAM, las reservas grandes van a la PSRAM (el monticulo propio de LVGL es de 64 KB)
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

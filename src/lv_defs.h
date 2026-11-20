// APRS LoRa+ Display - Copyright (C) 2026 Manuel - EA7JRS - GPL-3.0-or-later
// Ajustes de LVGL 9.6 (se incluye en todo con -include; no hay lv_conf.h). Lo que lleva '&' no se puede pasar con -D.
#pragma once
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565      // RGB565 como el panel (sustituye a LV_COLOR_DEPTH 16)
#define LV_FONT_DEFAULT (&lv_font_montserrat_20)
// Memoria de LVGL: en el ESP32-S3, toda a la PSRAM (src/lv_mem_psram.c). Con el malloc de la libreria C las reservas de menos
// de 4 KB van a la RAM interna, y los miles de objetos de la interfaz tipo emisora la agotaban al arrancar (abort en
// lock_init_generic). En el PC (arneses de prueba), el malloc de la libreria C.
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#else
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#endif
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

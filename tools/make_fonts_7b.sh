#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fuentes de la interfaz tipo emisora de la 7B (src/fonts/app_*.c): las de la app Android (Barlow, Barlow Condensed y
# Share Tech Mono, licencia SIL OFL 1.1, tools/assets/fonts/OFL-*.txt), con ASCII y Latin-1. Los iconos de LVGL
# (LV_SYMBOL_*) salen de la Montserrat del tamano mas cercano (--lv-fallback).
#
#   npm i lv_font_conv@1.5.3
#   tools/make_fonts_7b.sh
set -e
TTF=tools/assets/fonts
CONV=${LV_FONT_CONV:-./node_modules/.bin/lv_font_conv}
OUT=src/fonts
TEXT="0x20-0x7E,0xA0-0xFF,0x2013,0x2014,0x2022,0x2026,0x2039,0x203A"
ASCII="0x20-0x7E,0xB0"
font() {   # nombre ttf tamano rango [respaldo]
    fb=""; [ -n "$5" ] && fb="--lv-fallback $5"
    $CONV --bpp 4 --size $3 --font "$TTF/$2" -r "$4" --format lvgl --lv-include lvgl.h --no-compress $fb -o $OUT/$1.c
    sed -i '1i /* Fuente de la app Android (SIL OFL 1.1, tools/assets/fonts/). Generado por tools/make_fonts_7b.sh. */' $OUT/$1.c
}
font app_text_18 barlow_regular.ttf 18 "$TEXT" lv_font_montserrat_16
font app_text_20 barlow_regular.ttf 20 "$TEXT" lv_font_montserrat_20
font app_text_24 barlow_regular.ttf 24 "$TEXT" lv_font_montserrat_24
font app_semi_20 barlow_semibold.ttf 20 "$TEXT" lv_font_montserrat_20
font app_cond_16 barlow_condensed_semibold.ttf 16 "$TEXT" lv_font_montserrat_16
font app_cond_18 barlow_condensed_semibold.ttf 18 "$TEXT" lv_font_montserrat_16
font app_bold_18 barlow_condensed_bold.ttf 18 "$TEXT" lv_font_montserrat_16
font app_bold_22 barlow_condensed_bold.ttf 22 "$TEXT" lv_font_montserrat_20
font app_bold_26 barlow_condensed_bold.ttf 26 "$TEXT" lv_font_montserrat_24
font app_bold_34 barlow_condensed_bold.ttf 34 "$TEXT" lv_font_montserrat_24
font app_mono_20 share_tech_mono.ttf 20 "$TEXT"
font app_mono_28 share_tech_mono.ttf 28 "$TEXT"
font app_mono_40 share_tech_mono.ttf 40 "$ASCII"
font app_mono_64 share_tech_mono.ttf 64 "$ASCII"
ls -la $OUT/app_*

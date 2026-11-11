#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Fuentes de la interfaz (src/fonts/*.c): DejaVu Sans y DejaVu Sans Bold, como las de la pantalla del rotor
# (APRS-LoRa-Plus-Rotor, NOTICE_FONTS.md), con ASCII, Latin-1 (acentos, ñ, °) y unos simbolos. Los iconos de LVGL
# (LV_SYMBOL_*) que no tiene DejaVu salen de la Montserrat del mismo tamano (--lv-fallback).
#
#   npm i lv_font_conv@1.5.3
#   tools/make_fonts.sh [carpeta con DejaVuSans.ttf y DejaVuSans-Bold.ttf]   (por defecto /usr/share/fonts/truetype/dejavu)
set -e
TTF=${1:-/usr/share/fonts/truetype/dejavu}
CONV=${LV_FONT_CONV:-./node_modules/.bin/lv_font_conv}
OUT=src/fonts
mkdir -p $OUT
TEXT="0x20-0x7E,0xA0-0xFF,0x2013,0x2022,0x2026,0x25CF"
NUM="0x20,0x2D-0x39,0xB0"                                  # espacio - . / 0-9 °
font() {   # nombre ttf tamano rango [respaldo]
    fb=""; [ -n "$5" ] && fb="--lv-fallback $5"
    $CONV --bpp 4 --size $3 --font "$TTF/$2" -r "$4" --format lvgl --lv-include lvgl.h --no-compress $fb -o $OUT/$1.c
    sed -i '1i /* DejaVu Sans (licencia Bitstream Vera / DejaVu, ver src/fonts/NOTICE_FONTS.md). Generado por tools/make_fonts.sh. */' $OUT/$1.c
}
for s in 12 14 16 20 24; do font dejavu_$s DejaVuSans.ttf $s "$TEXT" lv_font_montserrat_$s; done
for s in 16 20 24; do font dejavu_bold_$s DejaVuSans-Bold.ttf $s "$TEXT" lv_font_montserrat_$s; done
font dejavu_bold_28 DejaVuSans-Bold.ttf 28 "$TEXT" lv_font_montserrat_24
for s in 32 44 60; do font dejavu_num_$s DejaVuSans-Bold.ttf $s "$NUM"; done
ls -la $OUT

# Reloj (entorno waveshare_lcd7b_hamclock): negritas pequenas y los digitos del reloj con ':' (src/hamclock/fonts)
OUT=src/hamclock/fonts
mkdir -p $OUT
for s in 12 14; do font dejavu_bold_$s DejaVuSans-Bold.ttf $s "$TEXT" lv_font_montserrat_$s; done
font dejavu_clock_40 DejaVuSans-Bold.ttf 40 "0x20,0x2D-0x3A"
ls -la $OUT
ls -la $OUT

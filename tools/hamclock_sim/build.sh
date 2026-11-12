#!/bin/sh
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compila el reloj para el PC con LVGL 9.6 y saca las capturas en PNG (hc_*.png, 1024x600).
#   tools/hamclock_sim/build.sh <carpeta de lvgl 9.6> <ArduinoJson>/src [carpeta de salida]
set -e
LV=$1; AJ=$2; OUT=${3:-/tmp/hamclock_sim}
mkdir -p $OUT/obj
DEF="-DLV_CONF_SKIP=1 -DLV_FONT_MONTSERRAT_12=1 -DLV_FONT_MONTSERRAT_14=1 -DLV_FONT_MONTSERRAT_16=1 -DLV_FONT_MONTSERRAT_20=1 \
 -DLV_FONT_MONTSERRAT_24=1 -DLV_USE_QRCODE=1 -DLV_USE_LOG=0 -include src/lv_defs.h -I$LV -Isrc -Isrc/hamclock -I$AJ"
if [ ! -f $OUT/liblvgl.a ]; then
    find $LV/src -name '*.c' > $OUT/srcs
    cat $OUT/srcs | xargs -P 8 -I{} sh -c 'o='$OUT'/obj/$(echo {} | md5sum | cut -c1-16).o; gcc -O1 -w '"$DEF"' -c {} -o $o'
    ar rcs $OUT/liblvgl.a $OUT/obj/*.o
fi
for f in src/fonts/*.c src/hamclock/fonts/*.c; do gcc -O1 -w $DEF -c $f -o $OUT/font_$(basename $f .c).o; done
SRC="tools/hamclock_sim/sim.cpp src/hamclock/hc_ui.cpp src/hamclock/hc_model.cpp src/hamclock/hc_rig.cpp src/hamclock/hc_map.cpp src/hamclock/hc_land.cpp"
g++ -std=c++17 -O1 -Wall -Wextra $DEF $SRC $OUT/font_*.o $OUT/liblvgl.a -lm -lpthread -o $OUT/sim
rm -f $OUT/hc_*.raw
$OUT/sim $OUT
python3 - $OUT <<'PY'
import sys, glob, numpy as np
from PIL import Image
for f in sorted(glob.glob(sys.argv[1] + "/hc_*.raw")):
    a = np.fromfile(f, dtype="<u2").reshape(600, 1024)
    rgb = np.dstack([((a >> 11) & 31) * 255 // 31, ((a >> 5) & 63) * 255 // 63, (a & 31) * 255 // 31]).astype("uint8")
    Image.fromarray(rgb).save(f[:-4] + ".png"); print(f[:-4] + ".png")
PY

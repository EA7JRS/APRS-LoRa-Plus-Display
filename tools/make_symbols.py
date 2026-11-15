#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Simbolos APRS de la interfaz tipo emisora (7B v2) en src/aprs_symbols.c: las tres hojas de aprs.fi (hessu/aprs-symbols,
# tools/assets/aprs-symbols, las mismas de la app Android; licencias en COPYRIGHT.md) reducidas a 28 px por simbolo, en
# ARGB8888 (16 x 6 simbolos por hoja: tabla primaria, secundaria y superposiciones). Los logotipos de marcas (/M Apple,
# /Z Microsoft, \K Kenwood) se dejan en blanco.
#
#   python3 tools/make_symbols.py
import sys
from PIL import Image

SRC = "tools/assets/aprs-symbols/aprs-symbols-48-%d.png"
# sin argumentos: 28 px (7B); "22": 22 px para la LCD-7 (src/aprs_symbols_22.c, aprs_sym22_*)
PX = int(sys.argv[1]) if len(sys.argv) > 1 else 28
SUF = "" if PX == 28 else str(PX)
CELL, COLS, ROWS = 48, 16, 6
BLANK = {(0, "M"), (0, "Z"), (1, "K")}

out = ["/* Simbolos APRS de aprs.fi (hessu/aprs-symbols, ver tools/assets/aprs-symbols/COPYRIGHT.md). Generado por tools/make_symbols.py. */",
       '#include <lvgl.h>', ""]
for sheet in range(3):
    im = Image.open(SRC % sheet).convert("RGBA")
    dst = Image.new("RGBA", (COLS * PX, ROWS * PX), (0, 0, 0, 0))
    for r in range(ROWS):
        for c in range(COLS):
            ch = chr(ord("!") + r * COLS + c)
            if (sheet, ch) in BLANK:
                continue
            cell = im.crop((c * CELL, r * CELL, (c + 1) * CELL, (r + 1) * CELL)).resize((PX, PX), Image.LANCZOS)
            dst.paste(cell, (c * PX, r * PX))
    data = bytearray()
    for y in range(dst.size[1]):
        for x in range(dst.size[0]):
            r_, g, b, a = dst.getpixel((x, y))
            data += bytes((b, g, r_, a))
    name = "aprs_sym%s_%d" % (SUF, sheet)
    out.append("static const uint8_t %s_map[] = {" % name)
    for i in range(0, len(data), 48):
        out.append(",".join(str(v) for v in data[i:i + 48]) + ",")
    out.append("};")
    out.append("const lv_image_dsc_t %s = {" % name)
    out.append("    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888, .flags = 0, .w = %d, .h = %d, .stride = %d, .reserved_2 = 0}," % (dst.size[0], dst.size[1], dst.size[0] * 4))
    out.append("    .data_size = sizeof(%s_map), .data = %s_map, .reserved = NULL, .reserved_2 = NULL};" % (name, name))
    out.append("")
dest = "src/aprs_symbols%s.c" % ("_" + SUF if SUF else "")
open(dest, "w").write("\n".join(out))
print(dest, sum(len(l) for l in out))

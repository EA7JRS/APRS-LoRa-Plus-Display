#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Imagenes de la interfaz (LVGL 8.4, LV_COLOR_DEPTH 16) en src/ui_assets.cpp:
#  - img_app_icon: icono de la app Android APRS LoRa+ (tools/assets/app_icon.png, el de primer plano del icono adaptativo sobre
#    #1C252E) con esquinas redondeadas, para la barra superior. LV_IMG_CF_TRUE_COLOR_ALPHA.
#  - img_splash: pantalla de bienvenida de la app (tools/assets/welcome_logo.jpg, copia de app/src/main/res/drawable/welcome_logo.jpg
#    de APRS-LoRa-Plus-Android) con los bordes fundidos con #131A22, en 256 colores (LV_IMG_CF_INDEXED_8BIT).
#
#   python3 tools/make_ui_assets.py
import argparse

from PIL import Image, ImageDraw

SPLASH_BG = (0x13, 0x1A, 0x22)


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def feather(img, bg, frac=0.16):
    w, h = img.size
    m = Image.new("L", (w, h))
    bw = max(1, int(min(w, h) * frac))
    px = m.load()
    for y in range(h):
        for x in range(w):
            d = min(x, y, w - 1 - x, h - 1 - y) / bw
            t = 1.0 if d >= 1 else d * d * (3 - 2 * d)
            px[x, y] = int(255 * t)
    return Image.composite(img, Image.new("RGB", (w, h), bg), m)


def carray(name, data):
    out = ["static const uint8_t %s[] = {" % name]
    for i in range(0, len(data), 32):
        out.append("    " + ",".join(str(b) for b in data[i:i + 32]) + ",")
    out.append("};")
    return out


def icon(src, size):
    im = Image.open(src).convert("RGBA").resize((size, size), Image.LANCZOS)
    m = Image.new("L", (size * 4, size * 4), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, size * 4 - 1, size * 4 - 1), radius=size * 4 // 5, fill=255)
    im.putalpha(m.resize((size, size), Image.LANCZOS))
    data = bytearray()
    for y in range(size):
        for x in range(size):
            r, g, b, a = im.getpixel((x, y))
            v = rgb565(r, g, b)
            data += bytes((v & 0xFF, v >> 8, a))          # LV_COLOR_16_SWAP 0: byte bajo, alto, alfa
    return size, size, data


def splash(src, h):
    img = Image.open(src).convert("RGB")
    w = round(img.size[0] * h / img.size[1])
    img = feather(img.resize((w, h), Image.LANCZOS), SPLASH_BG)
    q = img.quantize(colors=256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
    pal = q.getpalette()[:768]
    pal += [0] * (768 - len(pal))
    data = bytearray()
    for i in range(256):
        r, g, b = pal[i * 3:i * 3 + 3]
        data += bytes((b, g, r, 255))                     # lv_color32_t: azul, verde, rojo, alfa
    data += q.tobytes()
    return w, h, data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--icon", default="tools/assets/app_icon.png")
    ap.add_argument("--icon-size", type=int, default=40)
    ap.add_argument("--splash", default="tools/assets/welcome_logo.jpg")
    ap.add_argument("--splash-h", type=int, default=540)
    ap.add_argument("--out", default="src/ui_assets.cpp")
    a = ap.parse_args()
    iw, ih, idata = icon(a.icon, a.icon_size)
    sw, sh, sdata = splash(a.splash, a.splash_h)
    out = [
        "/*",
        " * Copyright (C) 2026 Manuel - EA7JRS",
        " * SPDX-License-Identifier: GPL-3.0-or-later",
        " *",
        " * Generado por tools/make_ui_assets.py (icono y pantalla de bienvenida de la app APRS LoRa+). No editar a mano.",
        " */",
        '#include "ui_assets.h"',
        "",
    ]
    out += carray("icon_data", idata)
    out += carray("splash_data", sdata)
    out += [
        "",
        "const lv_img_dsc_t img_app_icon = { { LV_IMG_CF_TRUE_COLOR_ALPHA, 0, 0, %d, %d }, sizeof(icon_data), icon_data };" % (iw, ih),
        "const lv_img_dsc_t img_splash = { { LV_IMG_CF_INDEXED_8BIT, 0, 0, %d, %d }, sizeof(splash_data), splash_data };" % (sw, sh),
        "const uint32_t SPLASH_BG_RGB = 0x%02X%02X%02X;" % SPLASH_BG,
        "",
    ]
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    print("written", a.out, "icon %dx%d, splash %dx%d (%d bytes)" % (iw, ih, sw, sh, len(sdata)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Mascara de tierra del mapa del reloj (src/hamclock/hc_land.cpp): 512x256, proyeccion equirrectangular, 1 bit por pixel.
# Datos: Natural Earth 1:110m (dominio publico) en TopoJSON, del paquete npm world-atlas:
#   npm i world-atlas && python3 tools/make_hamclock_map.py node_modules/world-atlas/land-110m.json
import json, sys
from PIL import Image, ImageDraw

S = 4
topo = json.load(open(sys.argv[1]))
sc, tr = topo["transform"]["scale"], topo["transform"]["translate"]
arcs = []
for a in topo["arcs"]:
    x = y = 0
    pts = []
    for dx, dy in a:
        x += dx; y += dy
        pts.append((x * sc[0] + tr[0], y * sc[1] + tr[1]))
    arcs.append(pts)

def ring(idx, W, H):
    out = []
    for i in idx:
        out += arcs[i] if i >= 0 else arcs[~i][::-1]
    return [((lon + 180) / 360 * W * S, (90 - lat) / 180 * H * S) for lon, lat in out]


def mask(W, H):
    img = Image.new("L", (W * S, H * S), 0)
    d = ImageDraw.Draw(img)
    for g in topo["objects"]["land"]["geometries"]:
        polys = g["arcs"] if g["type"] == "MultiPolygon" else [g["arcs"]]
        for poly in polys:
            for k, r in enumerate(poly):
                pts = ring(r, W, H)
                if len(pts) > 2:
                    d.polygon(pts, fill=0 if k else 255)
    img = img.resize((W, H), Image.LANCZOS).point(lambda v: 255 if v >= 128 else 0)
    px = img.load()
    data = bytearray()
    for y in range(H):
        for xb in range(W // 8):
            b = 0
            for i in range(8):
                if px[xb * 8 + i, y]:
                    b |= 0x80 >> i
            data.append(b)
    print("%dx%d: tierra %.1f %%" % (W, H, 100 * sum(bin(b).count("1") for b in data) / (W * H)))
    return "".join("    " + ",".join(str(b) for b in data[i:i + 32]) + ",\n" for i in range(0, len(data), 32))

with open("src/hamclock/hc_land.cpp", "w") as f:
    f.write("// APRS LoRa+ Display (reloj) - Copyright (C) 2026 Manuel - EA7JRS - GPL-3.0-or-later\n")
    f.write("// Mascara de tierra 512x256 (Natural Earth 1:110m, dominio publico). Generado por tools/make_hamclock_map.py.\n")
    f.write('#include "hc_map.h"\n\nnamespace hcmap {\nextern const uint8_t LAND[MAP_W / 8 * MAP_H] = {\n')
    f.write(mask(512, 256) + "};\n}\n")

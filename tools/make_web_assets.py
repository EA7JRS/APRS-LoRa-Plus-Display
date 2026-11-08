#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Ficheros del instalador web para APRS LoRa+ Display. El instalador es el del tracker (GitHub Pages de EA7JRS/APRS-LoRa-Plus):
# su workflow de release descarga web-site-display.zip de la release del Display con la misma version y lo pone en display/.
#
#   make_web_assets.py board --board ENV --chip esp32s3 --build-dir .pio/build/ENV --boot-app0 PATH --out DIR
#   make_web_assets.py site --root DIR
#
# Estructura (dentro de display/ en el sitio del instalador): como la del rotor (latest.json, firmware/<ENV>/*.bin,
# manifests/<ENV>.json para <esp-web-install-button>, ota/<ENV>.json). El Display no se actualiza por aire: "ota": false.
import argparse
import hashlib
import json
import os
import re
import shutil
import sys

CHIP_FAMILY = {"esp32s3": "ESP32-S3"}
BOOTLOADER_OFFSET = {"esp32s3": 0x0}


def read_version(src="platformio.ini"):
    with open(src, encoding="utf-8") as f:
        m = re.search(r'-DDISPLAY_VERSION=\\"([^"\\]+)\\"', f.read())
    if not m:
        sys.exit("DISPLAY_VERSION not found in " + src)
    return m.group(1)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def cmd_board(a):
    version = read_version()
    fw_dir = os.path.join(a.out, "firmware", a.board)
    os.makedirs(fw_dir, exist_ok=True)
    os.makedirs(os.path.join(a.out, "ota"), exist_ok=True)
    os.makedirs(os.path.join(a.out, "manifests"), exist_ok=True)
    src = {"bootloader.bin": os.path.join(a.build_dir, "bootloader.bin"), "partitions.bin": os.path.join(a.build_dir, "partitions.bin"),
           "boot_app0.bin": a.boot_app0, "firmware.bin": os.path.join(a.build_dir, "firmware.bin")}
    for name, path in src.items():
        shutil.copyfile(path, os.path.join(fw_dir, name))
    parts = [{"path": "../firmware/%s/%s" % (a.board, n), "offset": o} for n, o in
             (("bootloader.bin", BOOTLOADER_OFFSET[a.chip]), ("partitions.bin", 0x8000), ("boot_app0.bin", 0xE000), ("firmware.bin", 0x10000))]
    manifest = {"name": "APRS LoRa+ Display %s" % a.board, "version": version, "new_install_prompt_erase": False,
                "builds": [{"chipFamily": CHIP_FAMILY[a.chip], "parts": parts}]}
    with open(os.path.join(a.out, "manifests", a.board + ".json"), "w") as f:
        json.dump(manifest, f, indent=2)
    fw = os.path.join(fw_dir, "firmware.bin")
    info = {"board": a.board, "chip": a.chip, "version": version, "size": os.path.getsize(fw), "sha256": sha256(fw),
            "url": "firmware/%s/firmware.bin" % a.board, "ota": False}
    with open(os.path.join(a.out, "ota", a.board + ".json"), "w") as f:
        json.dump(info, f, indent=2)
    print("%s: version %s, %s" % (a.board, version, info["url"]))


def cmd_site(a):
    boards = {}
    ota_dir = os.path.join(a.root, "ota")
    for name in sorted(os.listdir(ota_dir)):
        if name.endswith(".json"):
            with open(os.path.join(ota_dir, name)) as f:
                d = json.load(f)
            boards[d["board"]] = {"version": d["version"], "chip": d["chip"], "ota": d["ota"]}
    if not boards:
        sys.exit("no boards in " + ota_dir)
    versions = {b["version"] for b in boards.values()}
    if len(versions) != 1:
        sys.exit("boards with different versions: %s" % sorted(versions))
    with open(os.path.join(a.root, "latest.json"), "w") as f:
        json.dump({"device": "display", "version": versions.pop(), "boards": boards}, f, indent=2)
    print("display site: %d boards" % len(boards))


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("board")
    b.add_argument("--board", required=True)
    b.add_argument("--chip", required=True, choices=sorted(CHIP_FAMILY))
    b.add_argument("--build-dir", required=True)
    b.add_argument("--boot-app0", required=True)
    b.add_argument("--out", required=True)
    b.set_defaults(fn=cmd_board)
    s = sub.add_parser("site")
    s.add_argument("--root", required=True)
    s.set_defaults(fn=cmd_site)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()

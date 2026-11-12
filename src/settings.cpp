/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "settings.h"
#ifdef UI_RADIO
#include <Preferences.h>

namespace cfg {

static Settings s;
Settings& get() { return s; }

static std::string str(Preferences& p, const char* k, const std::string& d) { return p.getString(k, d.c_str()).c_str(); }

void load() {
    Preferences p;
    if (!p.begin("disp7", true)) return;                    // primera vez: valores de serie
    s.theme = p.getInt("theme", s.theme);
    s.backlight = p.getInt("back", s.backlight);
    for (int i = 0; i < 5; i++) { char k[8]; snprintf(k, sizeof k, "key%d", i); s.keys[i] = str(p, k, s.keys[i]); }
    s.dispPage = p.getInt("disp", s.dispPage);
    s.dispHdgUp = p.getBool("hdgup", s.dispHdgUp);
    s.wifiSsid = str(p, "ssid", s.wifiSsid);
    s.wifiPass = str(p, "wpass", s.wifiPass);
    s.rotorLink = p.getInt("rlink", s.rotorLink);
    s.rotorHost = str(p, "rhost", s.rotorHost);
    s.rotorPin = str(p, "rpin", s.rotorPin);
    s.rotorBle = str(p, "rble", s.rotorBle);
    s.rotorFollow = p.getInt("rfol", s.rotorFollow);
    s.rotorFollowCall = str(p, "rfcall", s.rotorFollowCall);
    s.rotorStep = p.getFloat("rstep", s.rotorStep);
    s.catDoppler = p.getBool("catdop", s.catDoppler);
    s.dxHost = str(p, "dxhost", s.dxHost);
    s.dxPort = p.getInt("dxport", s.dxPort);
    s.dxCall = str(p, "dxcall", s.dxCall);
    uint32_t f = p.getUInt("dxflt", 0x3F);
    s.dxHuman = f & 1; s.dxRbnCw = f & 2; s.dxRbnDig = f & 4; s.dxCw = f & 8; s.dxPho = f & 16; s.dxDig = f & 32;
    s.dxBand = str(p, "dxband", s.dxBand);
    s.owmKey = str(p, "owm", s.owmKey);
    s.wxMinutes = p.getInt("wxmin", s.wxMinutes);
    s.wxToTracker = p.getBool("wxtr", s.wxToTracker);
    s.wxToRotor = p.getBool("wxrot", s.wxToRotor);
    s.warnSrc = p.getInt("wsrc", s.warnSrc);
    s.warnPopup = p.getInt("wpop", s.warnPopup);
    s.warnKm = p.getInt("wkm", s.warnKm);
    s.aemetKey = str(p, "aemet", s.aemetKey);
    s.maCountry = str(p, "macty", s.maCountry);
    s.maArea = str(p, "maarea", s.maArea);
    if (s.warnSrc < 0 || s.warnSrc > 2) s.warnSrc = 0;
    s.units = p.getInt("units", s.units);
    if (s.units < 0 || s.units > 2) s.units = 0;
    s.mapLayer = p.getInt("maplay", s.mapLayer);
    s.mapZoom = p.getInt("mapz", s.mapZoom);
    s.mapSea = p.getBool("mapsea", s.mapSea);
    s.arcgisKey = str(p, "arcgis", s.arcgisKey);
    if (s.mapLayer < 0 || s.mapLayer > 5) s.mapLayer = 0;
    if (s.mapZoom < 3 || s.mapZoom > 19) s.mapZoom = 12;
    p.end();
}

void save() {
    Preferences p;
    if (!p.begin("disp7", false)) return;
    p.putInt("theme", s.theme);
    p.putInt("back", s.backlight);
    for (int i = 0; i < 5; i++) { char k[8]; snprintf(k, sizeof k, "key%d", i); p.putString(k, s.keys[i].c_str()); }
    p.putInt("disp", s.dispPage);
    p.putBool("hdgup", s.dispHdgUp);
    p.putString("ssid", s.wifiSsid.c_str());
    p.putString("wpass", s.wifiPass.c_str());
    p.putInt("rlink", s.rotorLink);
    p.putString("rhost", s.rotorHost.c_str());
    p.putString("rpin", s.rotorPin.c_str());
    p.putString("rble", s.rotorBle.c_str());
    p.putInt("rfol", s.rotorFollow);
    p.putString("rfcall", s.rotorFollowCall.c_str());
    p.putFloat("rstep", s.rotorStep);
    p.putBool("catdop", s.catDoppler);
    p.putString("dxhost", s.dxHost.c_str());
    p.putInt("dxport", s.dxPort);
    p.putString("dxcall", s.dxCall.c_str());
    p.putUInt("dxflt", (s.dxHuman ? 1 : 0) | (s.dxRbnCw ? 2 : 0) | (s.dxRbnDig ? 4 : 0) | (s.dxCw ? 8 : 0) | (s.dxPho ? 16 : 0) | (s.dxDig ? 32 : 0));
    p.putString("dxband", s.dxBand.c_str());
    p.putString("owm", s.owmKey.c_str());
    p.putInt("wxmin", s.wxMinutes);
    p.putBool("wxtr", s.wxToTracker);
    p.putBool("wxrot", s.wxToRotor);
    p.putInt("wsrc", s.warnSrc);
    p.putInt("wpop", s.warnPopup);
    p.putInt("wkm", s.warnKm);
    p.putString("aemet", s.aemetKey.c_str());
    p.putString("macty", s.maCountry.c_str());
    p.putString("maarea", s.maArea.c_str());
    p.putInt("units", s.units);
    p.putInt("maplay", s.mapLayer);
    p.putInt("mapz", s.mapZoom);
    p.putBool("mapsea", s.mapSea);
    p.putString("arcgis", s.arcgisKey.c_str());
    p.end();
}

} // namespace cfg
#endif

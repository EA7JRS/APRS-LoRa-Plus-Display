/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Ajustes de la pantalla 7B v2 (interfaz tipo emisora): se guardan en la flash (Preferences "disp7", settings.cpp).
#pragma once
#include <string>

namespace cfg {

struct Settings {
    int theme = 0;                          // 0 oscuro, 1 claro (las dos paletas de la app Android)
    int backlight = 100;
    std::string keys[5] = {"SLIST", "MSG", "BCN", "DISP", "ROTOR"};   // teclas tactiles junto a [F]
    int dispPage = 0;                       // DISP: 0 brujula, 1 GPS, 2 altitud, 3 reloj
    bool dispHdgUp = false;
    // WiFi (DX Cluster, tiempo de internet, firmware del tracker, caja del rotor por TCP)
    std::string wifiSsid, wifiPass;
    // caja APRS LoRa+ Rotor
    int rotorLink = 0;                      // 0 apagado, 1 TCP (WiFi), 2 BLE
    std::string rotorHost = "APRS-Rotor.local", rotorPin = "123456", rotorBle;
    int rotorFollow = 0;                    // 0 no, 1 punteria del tracker (evento rot), 2 estacion elegida, 3 ultima estacion oida con posicion
    std::string rotorFollowCall;
    float rotorStep = 2.0f;                 // grados minimos entre ordenes al seguir
    bool catDoppler = true;                 // con el tracker en SAT: Doppler del satelite al CAT de la caja
    // DX Cluster
    std::string dxHost = "ea4ure.com", dxCall;
    int dxPort = 7300;
    bool dxHuman = true, dxRbnCw = true, dxRbnDig = true, dxCw = true, dxPho = true, dxDig = true;
    std::string dxBand;
    // tiempo de internet (OpenWeatherMap) al tracker y a la caja del rotor
    std::string owmKey;
    int wxMinutes = 15;
    bool wxToTracker = true, wxToRotor = true;
};

Settings& get();
void load();
void save();

} // namespace cfg

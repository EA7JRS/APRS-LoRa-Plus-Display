/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "dsc.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "nmea.h"

namespace plotter {

std::string DscCall::key() const {
    return std::to_string(format) + ":" + mmsi + ":" + std::to_string(category) + ":" + std::to_string(f4) + ":" + std::to_string(f5) + ":" +
           field6 + ":" + time + ":" + distressMmsi;
}

static bool digitsOnly(const std::string& s) {
    for (char c : s) if (!isdigit((unsigned char)c)) return false;
    return !s.empty();
}

bool dscPosition(const std::string& d, LatLon& out) {
    if (d.size() != 10 || !digitsOnly(d) || d == "9999999999") return false;
    const int latD = atoi(d.substr(1, 2).c_str()), latM = atoi(d.substr(3, 2).c_str());
    const int lonD = atoi(d.substr(5, 3).c_str()), lonM = atoi(d.substr(8, 2).c_str());
    if (latM >= 60 || lonM >= 60 || latD > 90 || lonD > 180) return false;
    double la = latD + latM / 60.0, lo = lonD + lonM / 60.0;
    switch (d[0]) {
        case '0': break;
        case '1': lo = -lo; break;
        case '2': la = -la; break;
        case '3': la = -la; lo = -lo; break;
        default: return false;
    }
    out = LatLon{la, lo};
    return true;
}

static int two(const std::string& s) { return s.empty() || !digitsOnly(s) ? -1 : atoi(s.c_str()); }
static std::string mmsi9(const std::string& s) {
    if (!digitsOnly(s)) return "";
    return s.size() == 10 ? s.substr(0, 9) : s;
}

bool parseDscSentence(const char* line, DscCall& out) {
    if (!line) return false;
    const char* p = line;
    if (*p == '$') p++;
    if (strlen(p) < 6 || strncmp(p + 2, "DSC,", 4) != 0) return false;
    if (strchr(line, '*') && !nmeaChecksumOk(line[0] == '$' ? line : (std::string("$") + line).c_str())) return false;
    std::vector<std::string> f;
    std::string cur;
    for (const char* q = p + 6; *q && *q != '*' && *q != '\r' && *q != '\n'; q++) {
        if (*q == ',') { f.push_back(cur); cur.clear(); } else cur += *q;
    }
    f.push_back(cur);
    if (f.size() < 10) return false;
    DscCall c;
    c.format = two(f[0]);
    c.mmsi = mmsi9(f[1]);
    c.category = two(f[2]);
    c.f4 = two(f[3]);
    c.f5 = two(f[4]);
    c.field6 = f[5];
    c.time = f[6].size() == 4 && digitsOnly(f[6]) && f[6] != "8888" ? f[6] : "";
    c.distressMmsi = mmsi9(f[7]);
    c.distressNature = two(f[8]);
    c.ack = f[9].empty() ? 'S' : f[9][0];
    if (c.format < 0 || c.mmsi.empty()) return false;
    if (c.format == 12 && c.category < 0) c.category = 12;
    c.hasPos = (c.format == 12 || c.category == 12 || c.field6.size() == 10) && dscPosition(c.field6, c.pos);
    out = c;
    return true;
}

const char* dscFormatEs(int f) {
    switch (f) {
        case 2: return "Llamada a zona";
        case 12: return "Alerta de socorro";
        case 14: return "Llamada a grupo";
        case 16: return "Todos los buques";
        case 20: return "Llamada individual";
        case 23: return "Llamada autom\xC3\xA1tica";
        default: return "Formato desconocido";
    }
}

const char* dscCategoryEs(int c) {
    switch (c) {
        case 0: return "Rutina";
        case 8: return "Seguridad";
        case 10: return "Urgencia";
        case 12: return "Socorro";
        default: return "Categor\xC3\xAD" "a desconocida";
    }
}

const char* dscNatureEs(int n) {
    switch (n) {
        case 0: return "Incendio, explosi\xC3\xB3n";
        case 1: return "V\xC3\xA1" "a de agua";
        case 2: return "Abordaje";
        case 3: return "Varada";
        case 4: return "Escora, peligro de zozobra";
        case 5: return "Hundimiento";
        case 6: return "Sin gobierno y a la deriva";
        case 7: return "Socorro sin especificar";
        case 8: return "Abandono del buque";
        case 9: return "Pirater\xC3\xAD" "a, ataque armado";
        case 10: return "Hombre al agua";
        case 12: return "Emisi\xC3\xB3n de radiobaliza";
        default: return "Naturaleza desconocida";
    }
}

const char* dscTelecommandEs(int t) {
    switch (t) {
        case 0: return "Voz FM s\xC3\xADmplex";
        case 1: return "Voz FM d\xC3\xBApl" "ex";
        case 3: return "Sondeo";
        case 4: return "No puede cumplir";
        case 5: return "Fin de llamada";
        case 6: return "Datos";
        case 9: return "Voz BLU";
        case 10: return "Acuse de socorro";
        case 12: return "Retransmisi\xC3\xB3n de socorro";
        case 13: return "T\xC3\xA9lex FEC";
        case 15: return "T\xC3\xA9lex ARQ";
        case 18: return "Prueba";
        case 21: return "Actualizar posici\xC3\xB3n";
        case 26: return "Sin informaci\xC3\xB3n";
        default: return "Telecomando desconocido";
    }
}

const char* dscMmsiKindEs(const std::string& m) {
    if (m.compare(0, 2, "00") == 0) return "estaci\xC3\xB3n costera";
    if (m.compare(0, 1, "0") == 0) return "grupo";
    if (m.compare(0, 3, "111") == 0) return "aeronave SAR";
    if (m.compare(0, 3, "970") == 0) return "AIS-SART";
    if (m.compare(0, 3, "972") == 0) return "MOB";
    if (m.compare(0, 3, "974") == 0) return "radiobaliza AIS";
    if (m.compare(0, 2, "98") == 0) return "embarcaci\xC3\xB3n auxiliar";
    if (m.compare(0, 2, "99") == 0) return "ayuda a la navegaci\xC3\xB3n";
    return "buque";
}

bool DscLog::add(const DscCall& c) {
    const std::string k = c.key();
    for (size_t i = 0; i < items_.size(); i++)
        if (items_[i].key() == k && (c.utc == 0 || items_[i].utc == 0 || c.utc - items_[i].utc < 600)) {
            DscCall m = items_[i];
            m.repeats++;
            if (c.utc) m.utc = c.utc;
            items_.erase(items_.begin() + (long)i);
            items_.insert(items_.begin(), m);
            seq_++;
            return false;
        }
    items_.insert(items_.begin(), c);
    if (items_.size() > kMax) items_.pop_back();
    seq_++;
    return true;
}

}  // namespace plotter

/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gpx.h"

#include <cstdio>
#include <cstdlib>

namespace plotter {

namespace {

std::string unescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '&') { o += s[i]; continue; }
        static const char* ent[] = {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;"};
        static const char rep[] = {'&', '<', '>', '"', '\''};
        bool hit = false;
        for (int k = 0; k < 5 && !hit; k++) {
            std::string e = ent[k];
            if (s.compare(i, e.size(), e) == 0) { o += rep[k]; i += e.size() - 1; hit = true; }
        }
        if (!hit) o += '&';
    }
    return o;
}

std::string escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// Valor de un atributo dentro del texto de una etiqueta; comillas simples o dobles
bool attr(const std::string& tag, const char* name, double& v) {
    std::string key = std::string(name) + "=";
    size_t p = 0;
    while ((p = tag.find(key, p)) != std::string::npos) {
        if (p > 0 && tag[p - 1] != ' ' && tag[p - 1] != '\t' && tag[p - 1] != '\n' && tag[p - 1] != '\r') { p += key.size(); continue; }   // "lat" dentro de otro nombre
        p += key.size();
        if (p >= tag.size() || (tag[p] != '"' && tag[p] != '\'')) return false;
        char q = tag[p];
        size_t e = tag.find(q, p + 1);
        if (e == std::string::npos) return false;
        std::string num = tag.substr(p + 1, e - p - 1);
        char* end;
        v = std::strtod(num.c_str(), &end);
        return end != num.c_str() && *end == 0;
    }
    return false;
}

std::string tagName(const std::string& tag) {
    size_t e = tag.find_first_of(" \t\r\n/");
    std::string n = tag.substr(0, e);
    size_t c = n.find(':');                                 // sin prefijo de espacio de nombres
    return c == std::string::npos ? n : n.substr(c + 1);
}

}  // namespace

bool gpxParse(const std::string& xml, GpxData& out) {
    out = GpxData();
    bool isGpx = false;
    GpxPoint* pt = nullptr;         // punto abierto
    GpxPath* path = nullptr;        // ruta o track abierto
    int skip = 0;                   // profundidad dentro de <extensions> o <metadata>
    std::vector<std::string> stack;
    size_t i = 0;
    while (i < xml.size()) {
        size_t lt = xml.find('<', i);
        if (lt == std::string::npos) break;
        if (xml.compare(lt, 4, "<!--") == 0) { size_t e = xml.find("-->", lt); if (e == std::string::npos) return false; i = e + 3; continue; }
        if (xml.compare(lt, 9, "<![CDATA[") == 0) { size_t e = xml.find("]]>", lt); if (e == std::string::npos) return false; i = e + 3; continue; }
        size_t gt = xml.find('>', lt);
        if (gt == std::string::npos) return false;
        std::string tag = xml.substr(lt + 1, gt - lt - 1);
        i = gt + 1;
        if (tag.empty() || tag[0] == '?' || tag[0] == '!') continue;
        bool closing = tag[0] == '/', selfClose = tag[tag.size() - 1] == '/';
        std::string name = tagName(closing ? tag.substr(1) : tag);

        if (closing) {
            if (stack.empty() || stack.back() != name) return false;       // mal anidado
            stack.pop_back();
            if (skip) { if (name == "extensions" || name == "metadata") skip--; continue; }
            if (name == "wpt" || name == "rtept" || name == "trkpt") pt = nullptr;
            else if (name == "rte" || name == "trk") path = nullptr;
            continue;
        }
        if (!selfClose) stack.push_back(name);
        if (name == "extensions" || name == "metadata") { if (!selfClose) skip++; continue; }
        if (skip) continue;

        if (name == "gpx") isGpx = true;
        else if (name == "wpt" || name == "rtept" || name == "trkpt") {
            GpxPoint p;
            if (!attr(tag, "lat", p.lat) || !attr(tag, "lon", p.lon) || p.lat < -90 || p.lat > 90 || p.lon < -180 || p.lon > 180) return false;
            if (name == "wpt") { out.waypoints.push_back(p); pt = selfClose ? nullptr : &out.waypoints.back(); }
            else {
                if (!path) return false;
                path->pts.push_back(p);
                pt = selfClose ? nullptr : &path->pts.back();
            }
        } else if (name == "rte") { out.routes.emplace_back(); path = selfClose ? nullptr : &out.routes.back(); }
        else if (name == "trk") { out.tracks.emplace_back(); path = selfClose ? nullptr : &out.tracks.back(); }
        else if ((name == "name" || name == "time") && !selfClose) {
            size_t e = xml.find('<', i);
            if (e == std::string::npos) return false;
            std::string text = trim(unescape(xml.substr(i, e - i)));
            // El nombre pertenece al elemento que lo contiene directamente
            const std::string parent = stack.size() >= 2 ? stack[stack.size() - 2] : "";
            if (name == "name") {
                if (pt && (parent == "wpt" || parent == "rtept" || parent == "trkpt")) pt->name = text;
                else if (path && (parent == "rte" || parent == "trk")) path->name = text;
            } else if (pt && (parent == "wpt" || parent == "rtept" || parent == "trkpt")) pt->time = text;
        }
    }
    return isGpx && stack.empty();
}

static void writePoint(std::string& o, const char* tag, const GpxPoint& p, const char* indent) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s<%s lat=\"%.7f\" lon=\"%.7f\">", indent, tag, p.lat, p.lon);
    o += buf;
    if (!p.time.empty()) o += "<time>" + escape(p.time) + "</time>";
    if (!p.name.empty()) o += "<name>" + escape(p.name) + "</name>";
    o += std::string("</") + tag + ">\n";
}

std::string gpxWrite(const GpxData& d) {
    std::string o = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<gpx version=\"1.1\" creator=\"APRS LoRa+ Plotter\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n";
    for (const auto& w : d.waypoints) writePoint(o, "wpt", w, "  ");
    for (const auto& r : d.routes) {
        o += "  <rte>\n";
        if (!r.name.empty()) o += "    <name>" + escape(r.name) + "</name>\n";
        for (const auto& p : r.pts) writePoint(o, "rtept", p, "    ");
        o += "  </rte>\n";
    }
    for (const auto& t : d.tracks) {
        o += "  <trk>\n";
        if (!t.name.empty()) o += "    <name>" + escape(t.name) + "</name>\n";
        o += "    <trkseg>\n";
        for (const auto& p : t.pts) writePoint(o, "trkpt", p, "      ");
        o += "    </trkseg>\n  </trk>\n";
    }
    o += "</gpx>\n";
    return o;
}

}  // namespace plotter

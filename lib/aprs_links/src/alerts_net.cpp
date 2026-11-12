/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "alerts_net.h"

#include <cmath>
#include <functional>

namespace alertsnet {

const char* countryOf(double lat, double lon) {
    if ((lat > 35.9 && lat < 43.9 && lon > -9.5 && lon < 4.5) || (lat > 27.5 && lat < 29.5 && lon > -18.3 && lon < -13.3)) return "ES";
    if ((lat > 24 && lat < 50 && lon > -125 && lon < -66) || (lat > 51 && lat < 72 && lon > -170 && lon < -129) ||
        (lat > 18.5 && lat < 22.5 && lon > -160.5 && lon < -154.5)) return "US";
    return "";
}

}  // namespace alertsnet

#if defined(ESP32) && defined(APRS_LINKS_ALERTS)
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <time.h>

namespace alertsnet {

static Config cfg;
static Status st;
static SemaphoreHandle_t lock = nullptr;
static volatile uint32_t cfgVer = 0;
static volatile bool refreshNow = false;
static double posLat = 0, posLon = 0;
static bool posOk = false;
static std::vector<plotter::Alert> result;
static plotter::SpaceWx resultSpace;
static bool resultNew = false;

static const char* UA = "APRSLoRaPlus/5 (+https://ea7jrs.github.io/APRS-LoRa-Plus/)";

// Recibe el cuerpo de la respuesta (HTTPClient::writeToStream ya quita el "chunked")
class Sink : public Stream {
public:
    explicit Sink(std::function<void(const char*, size_t)> f, size_t limit) : f_(f), limit_(limit) {}
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* b, size_t n) override {
        if (total_ + n > limit_) { over_ = true; return 0; }
        total_ += n;
        f_((const char*)b, n);
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    bool over() const { return over_; }
private:
    std::function<void(const char*, size_t)> f_;
    size_t limit_, total_ = 0;
    bool over_ = false;
};

static bool fetch(const char* url, std::function<void(const char*, size_t)> f, size_t limit, const char* accept = nullptr) {
    WiFiClientSecure tls;
    tls.setInsecure();                                       // cifrado sin comprobar el certificado (como el tiempo de OpenWeatherMap)
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(12000);
    http.setUserAgent(UA);
    if (!http.begin(tls, url)) return false;
    if (accept) http.addHeader("Accept", accept);
    const int code = http.GET();
    if (code == 204) { http.end(); return true; }
    if (code != 200) { http.end(); return false; }
    Sink sink(f, limit);
    const int n = http.writeToStream(&sink);
    http.end();
    return n >= 0 && !sink.over();
}

static bool fetchText(const char* url, std::string& out, size_t limit, const char* accept = nullptr) {
    out.clear();
    return fetch(url, [&](const char* b, size_t n) { out.append(b, n); }, limit, accept);
}

// XML por bloques: cada <tag> completo a "onBlock" sin guardar el documento
static bool fetchBlocks(const char* url, const char* tag, std::function<void(const std::string&)> onBlock, size_t limit = 4000000) {
    plotter::XmlBlockSplitter sp(tag, 12000);
    return fetch(url, [&](const char* b, size_t n) { sp.feed(b, n, onBlock); }, limit);
}

static int64_t nowUnix() {
    time_t t = time(nullptr);
    return t > 1700000000 ? (int64_t)t : 0;
}

static void collect(std::vector<plotter::Alert>& all, plotter::SpaceWx& sw, const Config& c, double lat, double lon, bool havePos, char* msg, size_t msgSize) {
    const int64_t now = nowUnix();
    int errors = 0;
    char failed[48] = "";
    auto fail = [&](const char* what) { errors++; if (!failed[0]) snprintf(failed, sizeof(failed), "%s", what); };
    auto active = [&](const plotter::Alert& a) { return !now || a.activeAt(now); };
    const char* iso = c.country[0] ? c.country : (havePos ? countryOf(lat, lon) : "");
    if (c.weather) {
        if (strcmp(iso, "ES") == 0) {
            const char* rss = havePos && lat < 30 ? plotter::kAemetRssCanarias : plotter::kAemetRssPeninsula;
            if (!fetchBlocks(rss, "item", [&](const std::string& b) {
                    plotter::Alert a;
                    if (plotter::aemetParseRssItem(b, a) && a.level >= 1 && active(a) && plotter::alertZoneMatch(a.area, c.zones)) all.push_back(a);
                })) fail("AEMET");
        } else if (iso[0]) {
            const char* feed = plotter::meteoAlarmFeedName(iso);
            if (feed) {
                std::string url = std::string(plotter::kMeteoAlarmFeed) + feed;
                if (!fetchBlocks(url.c_str(), "entry", [&](const std::string& b) {
                        plotter::Alert a;
                        if (plotter::meteoAlarmParseEntry(b, a) && a.level >= 1 && active(a) && plotter::alertZoneMatch(a.area, c.zones) && all.size() < 60) all.push_back(a);
                    })) fail("MeteoAlarm");
            }
        }
        if (strcmp(iso, "US") == 0 && havePos) {
            char url[120];
            snprintf(url, sizeof(url), "%s%.2f,%.2f", plotter::kNwsActive, lat, lon);
            std::string j;
            std::vector<plotter::Alert> v;
            if (fetchText(url, j, 400000, "application/geo+json") && plotter::nwsParse(j, v)) { for (auto& a : v) if (active(a)) all.push_back(a); }
            else fail("NWS");
        }
    }
    if (c.tsunami) {
        for (const char* u : {plotter::kTsunamiPtwc, plotter::kTsunamiNtwc}) {
            if (!fetchBlocks(u, "entry", [&](const std::string& b) {
                    plotter::Alert a;
                    if (!plotter::tsunamiParseEntry(b, a) || !active(a)) return;
                    const bool near = havePos && a.hasPos() && plotter::gcDistanceNm({lat, lon}, {a.lat, a.lon}) * 1.852 < 1500;
                    if (a.level >= 1 || near) all.push_back(a);
                })) fail("tsunami.gov");
        }
    }
    if (c.quakes) {
        std::vector<plotter::Alert> q;
        // USGS con filtro (solo los campos que hacen falta): la respuesta entera ocupa decenas de KB
        {
            WiFiClientSecure tls;
            tls.setInsecure();
            HTTPClient http;
            http.setConnectTimeout(8000);
            http.setTimeout(12000);
            http.setUserAgent(UA);
            http.useHTTP10(true);
            bool ok = false;
            if (http.begin(tls, plotter::kUsgs45Day) && http.GET() == 200) {
                JsonDocument filter;
                filter["features"][0]["id"] = true;
                filter["features"][0]["properties"]["mag"] = true;
                filter["features"][0]["properties"]["place"] = true;
                filter["features"][0]["properties"]["time"] = true;
                filter["features"][0]["properties"]["type"] = true;
                filter["features"][0]["geometry"]["coordinates"] = true;
                JsonDocument doc;
                if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
                    ok = true;
                    for (JsonObject f : doc["features"].as<JsonArray>()) {
                        JsonArray co = f["geometry"]["coordinates"];
                        if (co.size() < 2) continue;
                        plotter::Alert a;
                        a.src = plotter::AlertSrc::Usgs;
                        a.id = std::string("usgs/") + (const char*)(f["id"] | "");
                        char ev[24];
                        snprintf(ev, sizeof(ev), "M%.1f Earthquake", (double)(f["properties"]["mag"] | 0.0));
                        a.event = ev;
                        a.area = (const char*)(f["properties"]["place"] | "");
                        a.lat = co[1] | 0.0;
                        a.lon = co[0] | 0.0;
                        char tx[24];
                        snprintf(tx, sizeof(tx), "Depth %.0f km", std::fabs((double)(co[2] | 0.0)));
                        a.text = tx;
                        a.sent = a.onset = (int64_t)((double)(f["properties"]["time"] | 0.0) / 1000);
                        a.expires = a.onset ? a.onset + 86400 : 0;
                        q.push_back(a);
                    }
                }
            }
            http.end();
            if (!ok) fail("USGS");
        }
        if (havePos && now) {
            std::string j;
            if (fetchText(plotter::emscUrl(lat, lon, now - 86400).c_str(), j, 200000)) plotter::quakeParseEmsc(j, q);
            else fail("EMSC");
        }
        plotter::quakeSetLevels(q, {lat, lon}, havePos);
        for (auto& a : q) {
            const bool near = havePos && a.hasPos() && plotter::gcDistanceNm({lat, lon}, {a.lat, a.lon}) * 1.852 < 1000;
            if ((a.level >= 1 || near) && active(a)) all.push_back(a);
        }
    }
    if (c.space) {
        std::string j;
        plotter::SpaceWx w;
        bool any = false;
        if (fetchText(plotter::kSwpcScales, j, 20000)) any |= plotter::swpcParseScales(j, w);
        if (fetchText(plotter::kSwpcKp, j, 60000)) any |= plotter::swpcParseKp(j, w);
        for (const char* u : {plotter::kSwpcFlux, plotter::kSwpcMag, plotter::kSwpcWind}) if (fetchText(u, j, 4000)) any |= plotter::swpcParseSummary(j, w);
        if (fetchText(plotter::kSwpcAlerts, j, 200000)) plotter::swpcParseAlerts(j, now, all);
        if (any) { w.updated = now; sw = w; } else fail("SWPC");
    }
    if (errors) snprintf(msg, msgSize, "%u in force; failed: %s%s", (unsigned)all.size(), failed, errors > 1 ? "..." : "");
    else snprintf(msg, msgSize, "%u in force", (unsigned)all.size());
}

static void task(void*) {
    uint32_t next = 0, seen = 0xFFFFFFFFu;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        Config c;
        double la, lo;
        bool hp;
        xSemaphoreTake(lock, portMAX_DELAY);
        c = cfg; la = posLat; lo = posLon; hp = posOk;
        const bool force = refreshNow;
        refreshNow = false;
        const uint32_t v = cfgVer;
        xSemaphoreGive(lock);
        if (!c.on || WiFi.status() != WL_CONNECTED) continue;
        const uint32_t now = millis();
        if (v != seen) { seen = v; next = now + 15000; }        // configuracion nueva: a los 15 s (la WiFi y la hora se asientan)
        if (!force && (int32_t)(now - next) < 0) continue;
        std::vector<plotter::Alert> all;
        plotter::SpaceWx sw;
        char msg[72];
        collect(all, sw, c, la, lo, hp, msg, sizeof(msg));
        xSemaphoreTake(lock, portMAX_DELAY);
        result.swap(all);
        resultSpace = sw;
        resultNew = true;
        snprintf(st.msg, sizeof(st.msg), "%s", msg);
        st.lastMs = millis();
        if (strstr(msg, "failed")) st.fail++; else st.ok++;
        xSemaphoreGive(lock);
        next = millis() + (uint32_t)(c.minutes < 10 ? 10 : c.minutes) * 60000u;
    }
}

void begin() {
    if (lock) return;
    lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(task, "alerts", 16384, nullptr, 1, nullptr, 0);
}

void apply(const Config& c) {
    if (!lock) begin();
    xSemaphoreTake(lock, portMAX_DELAY);
    cfg = c;
    cfgVer++;
    xSemaphoreGive(lock);
}

void setPosition(double lat, double lon) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    posLat = lat; posLon = lon; posOk = true;
    xSemaphoreGive(lock);
}

void refresh() { refreshNow = true; }

bool take(std::vector<plotter::Alert>& out, plotter::SpaceWx& space) {
    if (!lock) return false;
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool n = resultNew;
    if (n) { out.swap(result); result.clear(); space = resultSpace; resultNew = false; }
    xSemaphoreGive(lock);
    return n;
}

const Status& status() { return st; }

}  // namespace alertsnet

#else

namespace alertsnet {
static Status st;
void begin() {}
void apply(const Config&) {}
void setPosition(double, double) {}
void refresh() {}
bool take(std::vector<plotter::Alert>&, plotter::SpaceWx&) { return false; }
const Status& status() { return st; }
}  // namespace alertsnet

#endif

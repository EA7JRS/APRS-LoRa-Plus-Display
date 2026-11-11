/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Reloj de radioaficionado con los datos de OpenHamClock: cabecera con la hora, meteorologia espacial, condiciones por banda,
 * DE / DX, mapa con la noche y los spots, POTA / SOTA, pases de satelites, DX cluster y noticias. Paleta de la app (PAL_APP
 * de src/ui.cpp). Al tocar un spot pasa a ser el DX del mapa; la rueda abre los ajustes. Con CAT (rigctld de Hamlib en los
 * ajustes) ensena la frecuencia y el modo de la radio y TUNE la lleva al spot o a la activacion elegidos.
 */
#include "hc_ui.h"
#include "hc_map.h"
#include <lvgl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define HC_FONT(name, font) LV_FONT_DECLARE(font) static const lv_font_t* name = &font;
HC_FONT(F12, dejavu_12)
HC_FONT(F14, dejavu_14)
HC_FONT(F16, dejavu_16)
HC_FONT(F20, dejavu_20)
HC_FONT(B12, dejavu_bold_12)
HC_FONT(B14, dejavu_bold_14)
HC_FONT(B16, dejavu_bold_16)
HC_FONT(B20, dejavu_bold_20)
HC_FONT(B28, dejavu_bold_28)
HC_FONT(FCLOCK, dejavu_clock_40)

#ifndef DISPLAY_VERSION
#define DISPLAY_VERSION "dev"
#endif

namespace hcui {

enum : uint32_t { BG = 0x131A22, CARD = 0x18222D, BORDER = 0x263443, FG = 0xE8EEF3, MUT = 0x9FB0BF, ACC = 0x28B5E1, RED = 0xFF6B6B,
                  GREEN = 0x3CE68A, YEL = 0xFFB347, FIELD = 0x0B1016, SEL = 0x1E2A36 };

// disposicion (pixeles)
constexpr int W = 1024, H = 600, TOP = 60, BOT = H - 32, FOOT = H - 24, LW = 216, RW = 262, GAP = 8;
constexpr int CX = 8 + LW + GAP, CW = W - 16 - LW - RW - 2 * GAP;            // columna central: 514
constexpr int RX = W - 8 - RW;
constexpr int N_ROWS = 15, ROW_H = 30;
constexpr int N_ACT = 9, N_PASS = 9;

static hc::Model* M = nullptr;
static hc::Config cfg;
static void (*saveCb)(const hc::Config&) = nullptr;
static void (*tuneCb)(double, const char*) = nullptr;
static bool hasDe = false;
static double deLat = 0, deLon = 0;

static lv_color_t C(uint32_t v) { return lv_color_hex(v); }

// ---------- piezas ----------
static lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_scrollable(o, false);
    return o;
}
static lv_obj_t* fill(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius = 0) {
    lv_obj_t* o = box(parent, x, y, w, h);
    lv_obj_set_style_bg_color(o, C(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}
static lv_obj_t* text(lv_obj_t* parent, int x, int y, const lv_font_t* f, uint32_t color, const char* s = "") {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, C(color), 0);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, s);
    return l;
}
// texto pegado a la derecha: ocupa de x a x + w
static lv_obj_t* textR(lv_obj_t* parent, int x, int y, int w, const lv_font_t* f, uint32_t color, const char* s = "") {
    lv_obj_t* l = text(parent, x, y, f, color, s);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    return l;
}
// tarjeta con titulo en mayusculas espaciadas (como la app) y una nota a la derecha; devuelve la tarjeta y deja la nota en *note
static lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h, const char* title, lv_obj_t** note = nullptr) {
    lv_obj_t* c = fill(parent, x, y, w, h, CARD, 8);
    lv_obj_set_style_border_color(c, C(BORDER), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_clip_corner(c, true, 0);
    lv_obj_t* t = text(c, 9, 5, B12, ACC, title);
    lv_obj_set_style_text_letter_space(t, 3, 0);
    lv_obj_t* n = textR(c, w - 132, 5, 120, F12, MUT);
    if (note) *note = n;
    fill(c, 0, 24, w, 1, BORDER);
    return c;
}
static void setText(lv_obj_t* l, const char* s) {
    if (strcmp(lv_label_get_text(l), s)) lv_label_set_text(l, s);
}
static void setColor(lv_obj_t* l, uint32_t c) { lv_obj_set_style_text_color(l, C(c), 0); }

// ---------- objetos ----------
static lv_obj_t *lCall, *lGrid, *lPos, *lClock, *lDate, *lLocal, *lHdr[3], *lWifi, *lRssi;
static lv_obj_t *lWx[6], *kpBar[hc::KP_HIST];
static lv_obj_t *lBandName[4], *lBandDay[4], *lBandNight[4];
static lv_obj_t *lDeDx[7], *btnTune, *lTune;
static lv_obj_t *mapBox, *mapImg, *lDeMap, *lDxMap;
static lv_obj_t *lActNote, *actDot[N_ACT], *lAct[N_ACT][4], *actBox[N_ACT];
static lv_obj_t *lPass[N_PASS][3];
static lv_obj_t *lSpotNote, *rowBox[N_ROWS], *rowBar[N_ROWS], *lRow[N_ROWS][5];
static lv_obj_t *lNews, *lSrv;
static lv_obj_t* settings = nullptr;

static uint16_t* mapBuf = nullptr;
static lv_image_dsc_t mapDsc;

// copia de lo que se ve en la lista del cluster (lo que se toca es lo que habia en pantalla, no lo que acabe de llegar)
static hc::Spot shown[N_ROWS];
static int nShown = 0;
static bool hasDx = false;
static char dxCall[14] = "";
static double dxLat = 0, dxLon = 0;
static bool mapDirty = true, dxDirty = true;
// lo que se sintoniza con TUNE: el ultimo spot o activacion que se ha tocado
static hc::Activation shownAct[N_ACT];
static bool actOn[N_ACT];
static bool hasTarget = false;
static double targetMhz = 0;
static char targetMode[6] = "";
static uint32_t tuneFlash = 0;                         // hasta cuando el boton dice SENT

// ---------- mapa ----------
static void drawMap(time_t now) {
    static hcmap::Marker mk[hc::MAX_SPOTS];
    hcmap::Scene s;
    s.now = now;
    s.hasDe = hasDe; s.deLat = deLat; s.deLon = deLon;
    s.hasDx = hasDx; s.dxLat = dxLat; s.dxLon = dxLon;
    int n = 0;
    for (int i = 0; i < M->nSpots; i++)
        if (M->spots[i].hasPos) mk[n++] = {M->spots[i].lat, M->spots[i].lon, hc::bandColor(M->spots[i].band)};
    s.markers = mk; s.nMarkers = n;
    hcmap::render(mapBuf, s);
    lv_obj_invalidate(mapImg);
    // etiquetas de DE y DX junto a su punto (a la izquierda si no caben a la derecha)
    auto place = [](lv_obj_t* l, bool on, double lat, double lon, const char* txt) {
        if (!on || !*txt) { lv_obj_set_hidden(l, true); return; }
        lv_obj_set_hidden(l, false);
        setText(l, txt);
        lv_obj_update_layout(l);
        int x, y, w = lv_obj_get_width(l);
        hcmap::toXY(lat, lon, x, y);
        int lx = x + 9 + w > hcmap::MAP_W ? x - 9 - w : x + 9, ly = y - 16 < 0 ? y + 4 : y - 16;
        lv_obj_set_pos(l, lx, ly);
    };
    place(lDeMap, hasDe, deLat, deLon, cfg.call);
    place(lDxMap, hasDx, dxLat, dxLon, dxCall);
    mapDirty = false;
}

// ---------- DE / DX ----------
static void fmtHm(char* out, size_t n, time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(out, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
}
static void refreshDeDx(time_t now, bool timeOk) {
    char b[64], r[8], s[8];
    char g4[5] = "";
    if (hasDe) hc::latLonToGrid(deLat, deLon, g4);
    snprintf(b, sizeof(b), "%s  %s", cfg.call, cfg.grid[0] ? cfg.grid : "----");
    setText(lDeDx[0], b);
    time_t tr, ts;
    if (hasDe && timeOk && hc::sunRiseSet(now, deLat, deLon, tr, ts)) {
        fmtHm(r, sizeof(r), tr); fmtHm(s, sizeof(s), ts);
        snprintf(b, sizeof(b), "%s - %s z", r, s);
    } else strcpy(b, "--");
    setText(lDeDx[1], b);
    dxDirty = false;
    if (!hasDx) {                                      // nada elegido, o algo sin posicion conocida (POTA / SOTA)
        setText(lDeDx[2], dxCall[0] ? dxCall : "tap a spot");
        setColor(lDeDx[2], dxCall[0] ? GREEN : MUT);
        for (int i = 3; i < 6; i++) setText(lDeDx[i], "--");
        return;
    }
    char g[5];
    hc::latLonToGrid(dxLat, dxLon, g);
    snprintf(b, sizeof(b), "%s  %s", dxCall, g);
    setText(lDeDx[2], b);
    setColor(lDeDx[2], GREEN);
    if (hasDe) {
        double d = hc::distanceKm(deLat, deLon, dxLat, dxLon), az = hc::bearingDeg(deLat, deLon, dxLat, dxLon);
        snprintf(b, sizeof(b), "SP %.0f\xC2\xB0  %.0f km", az, d);
        setText(lDeDx[3], b);
        snprintf(b, sizeof(b), "LP %.0f\xC2\xB0  %.0f km", fmod(az + 180, 360), 40030.0 - d);
        setText(lDeDx[4], b);
    } else { setText(lDeDx[3], "--"); setText(lDeDx[4], "--"); }
    if (timeOk) {       // hora solar del DX: el huso que le toca por longitud
        int off = (int)lround(dxLon / 15);
        fmtHm(r, sizeof(r), now + off * 3600);
        snprintf(b, sizeof(b), "%s (UTC%+d)", r, off);
        setText(lDeDx[5], b);
    } else setText(lDeDx[5], "--");
    dxDirty = false;
}

// ---------- CAT ----------
static void refreshRig() {
    if (!cfg.rig[0]) { setText(lDeDx[6], "not set"); setColor(lDeDx[6], MUT); return; }
    bool on = M->rigLink;
    if (!on) { setText(lDeDx[6], "offline"); setColor(lDeDx[6], RED); }
    else {
        char b[40];
        long long hz = (long long)llround(M->rigHz);      // 14.074.00 = MHz, kHz y decenas de Hz
        snprintf(b, sizeof(b), "%lld.%03lld.%02lld %s", hz / 1000000, (hz / 1000) % 1000, (hz / 10) % 100, M->rigMode);
        setText(lDeDx[6], b); setColor(lDeDx[6], ACC);
    }
    bool can = on && hasTarget;
    lv_obj_set_style_bg_color(btnTune, C(can ? ACC : SEL), 0);
    setColor(lTune, can ? BG : MUT);
}
static void setTarget(const char* freq, const char* mode) {
    targetMhz = atof(freq);
    snprintf(targetMode, sizeof(targetMode), "%s", mode);
    hasTarget = targetMhz > 0;
    if (cfg.rig[0]) refreshRig();
}
static void tuneClicked(lv_event_t*) {
    if (!tuneCb || !hasTarget || !M->rigLink) return;
    tuneCb(targetMhz, targetMode);
    setText(lTune, "SENT");
    tuneFlash = lv_tick_get() + 1200;
}

// ---------- DX cluster ----------
static void selectRow(int idx) {
    for (int i = 0; i < N_ROWS; i++) {
        bool on = i == idx;
        lv_obj_set_style_bg_opa(rowBox[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rowBox[i], on ? 1 : 0, 0);
    }
}
static void selectAct(int idx) {
    for (int i = 0; i < N_ACT; i++) {
        lv_obj_set_style_bg_opa(actBox[i], i == idx ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actBox[i], i == idx ? 1 : 0, 0);
    }
}
static void rowClicked(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= nShown) return;
    selectAct(-1);
    snprintf(dxCall, sizeof(dxCall), "%s", shown[i].dx);
    hasDx = shown[i].hasPos;          // sin posicion conocida se queda elegido en la lista, pero sin trayecto
    dxLat = shown[i].lat; dxLon = shown[i].lon;
    selectRow(i);
    setTarget(shown[i].freq, shown[i].mode);
    mapDirty = dxDirty = true;
}
// una activacion POTA / SOTA: se queda como DX (sin posicion, asi que sin trayecto) y como destino de TUNE
static void actClicked(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!actOn[i]) return;
    snprintf(dxCall, sizeof(dxCall), "%s", shownAct[i].call);
    hasDx = false;
    selectRow(-1);
    selectAct(i);
    setTarget(shownAct[i].freq, shownAct[i].mode);
    mapDirty = dxDirty = true;
}
static void refreshSpots() {
    nShown = M->nSpots < N_ROWS ? M->nSpots : N_ROWS;
    int sel = -1;
    for (int i = 0; i < N_ROWS; i++) {
        if (i >= nShown) { lv_obj_set_hidden(rowBox[i], true); continue; }
        const hc::Spot& s = shown[i] = M->spots[i];
        lv_obj_set_hidden(rowBox[i], false);
        uint32_t bc = hc::bandColor(s.band);
        lv_obj_set_style_bg_color(rowBar[i], C(bc), 0);
        setText(lRow[i][0], s.freq); setColor(lRow[i][0], bc);
        setText(lRow[i][1], s.dx);
        char de[20];
        snprintf(de, sizeof(de), "de %s", s.de);
        setText(lRow[i][2], de);
        setText(lRow[i][3], s.mode);
        setText(lRow[i][4], s.time);
        if (dxCall[0] && !strcmp(dxCall, s.dx) && sel < 0) sel = i;
    }
    selectRow(sel);
    char b[32];
    snprintf(b, sizeof(b), "%d spots", M->nSpots);
    setText(lSpotNote, M->nSpots ? b : "waiting...");
    mapDirty = true;
}

// ---------- resto de paneles ----------
static uint32_t condColor(const char* c) {
    return !strcmp(c, "Good") ? GREEN : !strcmp(c, "Fair") ? YEL : !strcmp(c, "Poor") ? RED : MUT;
}
static void pill(lv_obj_t* l, const char* txt) {
    uint32_t c = condColor(txt);
    setText(l, *txt ? txt : "--");
    setColor(l, c);
    lv_obj_set_style_bg_color(l, C(c), 0);
    lv_obj_set_style_border_color(l, C(c), 0);
}
static void refreshSolar() {
    char b[24];
    auto num = [&](lv_obj_t* l, bool ok, const char* fmt, double v, uint32_t color) {
        if (ok) snprintf(b, sizeof(b), fmt, v); else strcpy(b, "--");
        setText(l, b); setColor(l, ok ? color : MUT);
    };
    uint32_t kc = M->kp < 4 ? GREEN : M->kp < 5 ? YEL : RED;
    num(lWx[0], M->sfi >= 0, "%.0f", M->sfi, M->sfi >= 120 ? GREEN : M->sfi >= 90 ? YEL : RED);
    num(lWx[1], M->ssn >= 0, "%.0f", M->ssn, FG);
    num(lWx[2], M->kp >= 0, "%.1f", M->kp, kc);
    num(lWx[3], M->aIndex >= 0, "%.0f", M->aIndex, M->aIndex < 15 ? GREEN : M->aIndex < 30 ? YEL : RED);
    setText(lWx[4], M->xray[0] ? M->xray : "--");
    setColor(lWx[4], !M->xray[0] ? MUT : (M->xray[0] == 'X' ? RED : M->xray[0] == 'M' ? YEL : GREEN));
    num(lWx[5], M->wind >= 0, "%.0f", M->wind, M->wind < 500 ? FG : M->wind < 700 ? YEL : RED);
    num(lHdr[0], M->sfi >= 0, "%.0f", M->sfi, GREEN);
    num(lHdr[1], M->ssn >= 0, "%.0f", M->ssn, FG);
    num(lHdr[2], M->kp >= 0, "%.0f", M->kp, kc);
    for (int i = 0; i < hc::KP_HIST; i++) {
        int k = i - (hc::KP_HIST - M->kpN);            // las barras se llenan desde la derecha
        if (k < 0) { lv_obj_set_hidden(kpBar[i], true); continue; }
        float v = M->kpHist[k];
        int h = 4 + (int)(v * 4);
        if (h > 38) h = 38;
        lv_obj_set_hidden(kpBar[i], false);
        lv_obj_set_size(kpBar[i], 9, h);
        lv_obj_set_y(kpBar[i], 160 - h);
        lv_obj_set_style_bg_color(kpBar[i], C(v < 4 ? GREEN : v < 5 ? YEL : RED), 0);
    }
    for (int i = 0; i < 4; i++) {
        bool on = i < M->nBands;
        // "80m-40m" -> "80-40m"
        char name[12] = "";
        if (on) {
            size_t n = 0;
            for (const char* p = M->bands[i].name; *p && n < sizeof(name) - 1; p++)
                if (!(*p == 'm' && p[1] == '-')) name[n++] = *p;
            name[n] = 0;
        }
        setText(lBandName[i], on ? name : "--");
        pill(lBandDay[i], on ? M->bands[i].day : "");
        pill(lBandNight[i], on ? M->bands[i].night : "");
    }
}
static void refreshActs() {
    selectAct(-1);
    // POTA y SOTA alternados
    int ip = 0, is = 0;
    for (int i = 0; i < N_ACT; i++) {
        const hc::Activation* a = nullptr;
        bool wantSota = (i % 2) == 1;
        if (wantSota && is < M->nSota) a = &M->sota[is++];
        else if (ip < M->nPota) a = &M->pota[ip++];
        else if (is < M->nSota) a = &M->sota[is++];
        actOn[i] = a != nullptr;
        if (a) shownAct[i] = *a;
        if (!a) {
            lv_obj_set_hidden(actDot[i], true);
            for (auto l : lAct[i]) setText(l, "");
            continue;
        }
        lv_obj_set_hidden(actDot[i], false);
        lv_obj_set_style_bg_color(actDot[i], C(a->sota ? YEL : GREEN), 0);
        setText(lAct[i][0], a->call); setText(lAct[i][1], a->ref); setText(lAct[i][2], a->freq); setText(lAct[i][3], a->mode);
    }
    char b[32];
    snprintf(b, sizeof(b), "%d active", M->totalPota + M->totalSota);
    setText(lActNote, b);
}
static void refreshPasses(time_t now) {
    for (int i = 0; i < N_PASS; i++) {
        if (i >= M->nPasses) { for (auto l : lPass[i]) setText(l, i == 0 && l == lPass[0][1] ? "no data yet" : ""); continue; }
        const hc::Pass& p = M->passes[i];
        char b[24];
        long min = (long)((p.aos - now + 59) / 60);
        uint32_t c = FG;
        if (min <= 0) { strcpy(b, "now"); c = GREEN; }
        else if (min < 60) { snprintf(b, sizeof(b), "in %ld min", min); c = min <= 15 ? GREEN : FG; }
        else { char hm[8]; fmtHm(hm, sizeof(hm), p.aos); snprintf(b, sizeof(b), "%s z", hm); }
        if (p.maxEl < 20) c = MUT;
        setText(lPass[i][0], p.name); setColor(lPass[i][0], c);
        setText(lPass[i][1], b);
        snprintf(b, sizeof(b), "max %.0f\xC2\xB0", p.maxEl);
        setText(lPass[i][2], b); setColor(lPass[i][2], c);
    }
}
static void refreshLink() {
    if (!M->wifi) { setText(lWifi, "No WiFi"); setColor(lWifi, RED); setText(lRssi, cfg.ssid[0] ? "connecting" : "see settings"); }
    else {
        char b[24];
        snprintf(b, sizeof(b), "%d dBm", M->rssi);
        setText(lWifi, "WiFi"); setColor(lWifi, M->status[0] ? YEL : GREEN); setText(lRssi, b);
    }
    // a la derecha del pie: el servidor, o el ultimo error
    const char* host = strstr(cfg.server, "://");
    setText(lSrv, M->status[0] ? M->status : (host ? host + 3 : cfg.server));
    setColor(lSrv, M->status[0] ? YEL : MUT);
}

// ---------- ajustes ----------
static lv_obj_t *kb, *ta[7];
static void taFocus(lv_event_t* e) { lv_keyboard_set_textarea(kb, (lv_obj_t*)lv_event_get_target(e)); }
static void closeSettings(lv_event_t*) {
    if (settings) { lv_obj_delete_async(settings); settings = nullptr; }
}
static void saveSettings(lv_event_t*) {
    hc::Config c = cfg;
    auto get = [](char* dst, size_t n, lv_obj_t* t) { snprintf(dst, n, "%s", lv_textarea_get_text(t)); };
    get(c.call, sizeof(c.call), ta[0]); get(c.grid, sizeof(c.grid), ta[1]); get(c.tz, sizeof(c.tz), ta[2]);
    get(c.ssid, sizeof(c.ssid), ta[3]); get(c.pass, sizeof(c.pass), ta[4]); get(c.server, sizeof(c.server), ta[5]);
    get(c.rig, sizeof(c.rig), ta[6]);
    for (char* p = c.call; *p; p++) *p = (char)toupper((unsigned char)*p);
    size_t n = strlen(c.server);
    while (n && c.server[n - 1] == '/') c.server[--n] = 0;
    if (saveCb) saveCb(c);
    closeSettings(nullptr);
}
void openSettings() {
    if (settings) return;
    settings = fill(lv_screen_active(), 0, 0, W, H, BG);
    lv_obj_set_clickable(settings, true);                 // que los toques no lleguen a lo de debajo
    text(settings, 16, 12, B20, FG, "Settings");
    // About: version, copyright, fuentes y QR de donativos (PayPal.me de EA7JRS), como en el resto de pantallas APRS LoRa+
    text(settings, 16, 226, B12, FG, "APRS LoRa+ Display " DISPLAY_VERSION " \xC2\xB7 \xC2\xA9 2026 Manuel - EA7JRS");
    text(settings, 16, 245, F12, MUT, "Data: OpenHamClock, NOAA, N0NBH, POTA, SOTA");
    text(settings, 16, 264, F12, ACC, "Donate: paypal.me/ea7jrs  \xC2\xB7  GPL-3.0-or-later");
    lv_obj_t* qw = fill(settings, 878, 190, 0, 0, GREEN, 4);           // marco con el degradado del logo (verde > cian)
    lv_obj_set_size(qw, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_grad_color(qw, C(ACC), 0);
    lv_obj_set_style_bg_grad_dir(qw, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_pad_all(qw, 4, 0);
    lv_obj_t* qr = lv_qrcode_create(qw);
    lv_qrcode_set_size(qr, 84);
    lv_qrcode_set_dark_color(qr, C(BG));
    lv_qrcode_set_light_color(qr, lv_color_white());
    static const char kDonate[] = "https://paypal.me/ea7jrs";
    lv_qrcode_update(qr, kDonate, sizeof kDonate - 1);
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);              // margen blanco: lo piden los lectores de QR
    lv_obj_set_style_border_width(qr, 5, 0);
    static const char* names[7] = {"Callsign", "Locator", "Time zone (POSIX)", "WiFi network", "WiFi password", "OpenHamClock server",
                                   "Radio CAT: Hamlib rigctld host[:port] (empty = none)"};
    const char* vals[7] = {cfg.call, cfg.grid, cfg.tz, cfg.ssid, cfg.pass, cfg.server, cfg.rig};
    static const int maxLen[7] = {11, 6, 47, 32, 64, 95, 63};
    kb = lv_keyboard_create(settings);
    lv_obj_set_size(kb, W, 300);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(kb, F20, 0);
    for (int i = 0; i < 7; i++) {                       // tres campos a la izquierda y cuatro a la derecha
        int x = i < 3 ? 16 : 436, y = 48 + (i < 3 ? i : i - 3) * 56;
        text(settings, x, y, B12, MUT, names[i]);
        ta[i] = lv_textarea_create(settings);
        lv_obj_set_pos(ta[i], x, y + 17);
        lv_obj_set_size(ta[i], 400, 36);
        lv_textarea_set_one_line(ta[i], true);
        lv_textarea_set_max_length(ta[i], maxLen[i]);
        lv_textarea_set_text(ta[i], vals[i]);
        lv_obj_set_style_text_font(ta[i], F16, 0);
        lv_obj_set_style_bg_color(ta[i], C(FIELD), 0);
        lv_obj_set_style_text_color(ta[i], C(FG), 0);
        lv_obj_set_style_border_color(ta[i], C(BORDER), 0);
        lv_obj_set_style_border_color(ta[i], C(ACC), LV_STATE_FOCUSED);
        lv_obj_set_style_pad_ver(ta[i], 6, 0);
        lv_obj_add_event_cb(ta[i], taFocus, LV_EVENT_FOCUSED, nullptr);
    }
    lv_keyboard_set_textarea(kb, ta[cfg.ssid[0] ? 0 : 3]);
    lv_obj_add_state(ta[cfg.ssid[0] ? 0 : 3], LV_STATE_FOCUSED);
    auto button = [](int y, const char* txt, uint32_t bg, uint32_t fg, lv_event_cb_t cb) {
        lv_obj_t* b = lv_button_create(settings);
        lv_obj_set_pos(b, 864, y);
        lv_obj_set_size(b, 144, 48);
        lv_obj_set_style_bg_color(b, C(bg), 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, B16, 0);
        lv_obj_set_style_text_color(l, C(fg), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    };
    button(65, "Save", ACC, BG, saveSettings);
    button(125, "Cancel", SEL, FG, closeSettings);
}
static void gearClicked(lv_event_t*) { openSettings(); }

// ---------- montaje ----------
void begin(hc::Model* model, const hc::Config* c, void (*onSave)(const hc::Config&), void (*onTune)(double, const char*)) {
    M = model;
    cfg = *c;
    saveCb = onSave;
    tuneCb = onTune;
    hasDe = hc::gridToLatLon(cfg.grid, deLat, deLon);

    // tema oscuro de LVGL para lo que no se pinta a mano (teclado y campos de los ajustes)
    lv_display_t* disp = lv_display_get_default();
    lv_display_set_theme(disp, lv_theme_default_init(disp, C(ACC), C(YEL), true, F16));
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, C(BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    // cabecera
    fill(scr, 0, 52, W, 1, BORDER);
    lCall = text(scr, 14, 9, B28, YEL, cfg.call[0] ? cfg.call : "NOCALL");
    lv_obj_update_layout(lCall);
    int gx = 14 + lv_obj_get_width(lCall) + 14;                  // detras del indicativo, mida lo que mida
    lGrid = text(scr, gx, 10, B14, ACC, cfg.grid[0] ? cfg.grid : "----");
    lPos = text(scr, gx, 30, F12, MUT, "");
    if (hasDe) {
        char b[32];
        snprintf(b, sizeof(b), "%.2f%c  %.2f%c", fabs(deLat), deLat >= 0 ? 'N' : 'S', fabs(deLon), deLon >= 0 ? 'E' : 'W');
        lv_label_set_text(lPos, b);
    }
    text(scr, 330, 10, B12, MUT, "LOCAL");
    lLocal = text(scr, 330, 26, B16, GREEN, "--:--");
    lClock = text(scr, 0, 5, FCLOCK, FG, "--:--:--");
    lv_obj_set_width(lClock, 200);
    lv_obj_set_style_text_align(lClock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_x(lClock, 412);
    text(scr, 618, 10, B12, ACC, "UTC");
    lDate = text(scr, 618, 28, F12, MUT, "");
    static const char* hk[3] = {"SFI", "SSN", "Kp"};
    for (int i = 0; i < 3; i++) {
        text(scr, 750 + i * 56, 10, B12, MUT, hk[i]);
        lHdr[i] = text(scr, 750 + i * 56, 26, B16, FG, "--");
    }
    lWifi = text(scr, 916, 10, B12, RED, "No WiFi");
    lRssi = text(scr, 916, 28, F12, MUT, "");
    lv_obj_t* gear = box(scr, 980, 6, 40, 40);
    lv_obj_set_clickable(gear, true);
    lv_obj_set_ext_click_area(gear, 8);
    lv_obj_add_event_cb(gear, gearClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* gl = text(gear, 0, 0, F20, MUT, LV_SYMBOL_SETTINGS);
    lv_obj_center(gl);

    // izquierda: meteorologia espacial
    lv_obj_t* note;
    lv_obj_t* c1 = card(scr, 8, TOP, LW, 168, "SPACE WX", &note);
    lv_label_set_text(note, "NOAA");
    static const char* wk[6] = {"SFI", "SSN", "Kp", "A", "X-ray", "Wind"};
    for (int i = 0; i < 6; i++) {
        int x = 10 + (i % 3) * 68, y = 31 + (i / 3) * 44;
        text(c1, x, y, B12, MUT, wk[i]);
        lWx[i] = text(c1, x, y + 15, B20, MUT, "--");
    }
    textR(c1, LW - 82, 118, 70, F12, MUT, "Kp 48 h");
    for (int i = 0; i < hc::KP_HIST; i++) {
        kpBar[i] = fill(c1, 10 + (int)(i * 12.4f), 150, 9, 10, GREEN, 1);
        lv_obj_set_hidden(kpBar[i], true);
    }
    // condiciones por banda
    int y0 = TOP + 168 + GAP;
    lv_obj_t* c2 = card(scr, 8, y0, LW, 150, "BANDS", &note);
    lv_label_set_text(note, "day / night");
    for (int i = 0; i < 4; i++) {
        int y = 34 + i * 28;
        lBandName[i] = text(c2, 10, y + 2, B14, FG, "--");
        for (int j = 0; j < 2; j++) {
            lv_obj_t* l = text(c2, 90 + j * 60, y, B12, MUT, "--");
            lv_obj_set_size(l, 54, 22);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(l, 3, 0);
            lv_obj_set_style_radius(l, 5, 0);
            lv_obj_set_style_bg_opa(l, LV_OPA_20, 0);
            lv_obj_set_style_border_width(l, 1, 0);
            (j ? lBandNight : lBandDay)[i] = l;
        }
    }
    // DE / DX
    int y1 = y0 + 150 + GAP;
    lv_obj_t* c3 = card(scr, 8, y1, LW, BOT - y1, "DE / DX");
    static const char* dk[7] = {"DE", "Sun", "DX", "Path", "", "DX time", "Rig"};
    for (int i = 0; i < 7; i++) {
        int y = 31 + i * 19;
        text(c3, 10, y, B12, MUT, dk[i]);
        lDeDx[i] = text(c3, 74, y, (i == 0 || i == 2 || i == 6) ? B12 : F12, i == 0 ? YEL : i == 4 ? MUT : FG, "--");
    }
    // TUNE: lleva la radio (CAT) al spot o a la activacion elegidos; solo esta si hay rigctld en los ajustes
    btnTune = fill(c3, LW - 76, 2, 68, 20, SEL, 5);
    lv_obj_set_clickable(btnTune, true);
    lv_obj_set_ext_click_area(btnTune, 10);
    lv_obj_add_event_cb(btnTune, tuneClicked, LV_EVENT_CLICKED, nullptr);
    lTune = text(btnTune, 0, 0, B12, MUT, "TUNE");
    lv_obj_center(lTune);
    lv_obj_set_hidden(btnTune, !cfg.rig[0]);

    // centro: mapa
    mapBox = fill(scr, CX, TOP, CW, hcmap::MAP_H + 2, FIELD, 0);
    lv_obj_set_style_border_color(mapBox, C(BORDER), 0);
    lv_obj_set_style_border_width(mapBox, 1, 0);
    if (!mapBuf) mapBuf = (uint16_t*)lv_malloc(hcmap::MAP_W * hcmap::MAP_H * 2);
    memset(&mapDsc, 0, sizeof(mapDsc));
    mapDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    mapDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    mapDsc.header.w = hcmap::MAP_W;
    mapDsc.header.h = hcmap::MAP_H;
    mapDsc.header.stride = hcmap::MAP_W * 2;
    mapDsc.data_size = hcmap::MAP_W * hcmap::MAP_H * 2;
    mapDsc.data = (const uint8_t*)mapBuf;
    mapImg = lv_image_create(mapBox);
    lv_obj_set_pos(mapImg, 0, 0);
    lv_image_set_src(mapImg, &mapDsc);
    lDeMap = text(mapBox, 0, 0, B12, YEL);
    lDxMap = text(mapBox, 0, 0, B12, GREEN);
    static const hc::Band legend[] = {hc::B80, hc::B40, hc::B30, hc::B20, hc::B17, hc::B15, hc::B12, hc::B10, hc::B6};
    int lx = 6;
    for (hc::Band b : legend) {
        lv_obj_t* l = text(mapBox, lx, hcmap::MAP_H - 19, B12, FIELD, hc::bandName(b));
        lv_obj_set_size(l, 36, 16);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_radius(l, 4, 0);
        lv_obj_set_style_bg_color(l, C(hc::bandColor(b)), 0);
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
        lx += 40;
    }

    // bajo el mapa: POTA / SOTA y satelites
    // la tarjeta de POTA / SOTA lleva cuatro columnas y la de satelites tres: 30 mas de ancho para la primera
    int y2 = TOP + hcmap::MAP_H + 2 + GAP, half = (CW - GAP) / 2, hw = half + 30;
    lv_obj_t* c4 = card(scr, CX, y2, hw, BOT - y2, "POTA / SOTA", &lActNote);
    for (int i = 0; i < N_ACT; i++) {
        int y = 31 + i * 23;
        actDot[i] = fill(c4, 9, y + 4, 7, 7, GREEN, 4);
        lv_obj_set_hidden(actDot[i], true);
        lAct[i][0] = text(c4, 21, y, B12, FG);
        lAct[i][1] = text(c4, 94, y, F12, MUT);
        lAct[i][2] = textR(c4, hw - 99, y, 58, B12, ACC);
        lAct[i][3] = textR(c4, hw - 37, y, 29, F12, MUT);
        lv_obj_set_width(lAct[i][0], 71); lv_label_set_long_mode(lAct[i][0], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(lAct[i][1], hw - 99 - 94); lv_label_set_long_mode(lAct[i][1], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_height(lAct[i][0], 16); lv_obj_set_height(lAct[i][1], 16);
        // fondo de la fila, para tocarla (los textos no recogen el toque) y marcarla al elegirla
        actBox[i] = fill(c4, 3, y - 4, hw - 8, 23, SEL, 5);
        lv_obj_set_style_bg_opa(actBox[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(actBox[i], C(GREEN), 0);
        lv_obj_move_background(actBox[i]);
        lv_obj_set_clickable(actBox[i], true);
        lv_obj_add_event_cb(actBox[i], actClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    int sx = CX + hw + GAP;
    hw = CW - GAP - hw;
    lv_obj_t* c5 = card(scr, sx, y2, hw, BOT - y2, "SATELLITES", &note);
    lv_label_set_text(note, "next passes");
    for (int i = 0; i < N_PASS; i++) {
        int y = 31 + i * 23;
        lPass[i][0] = text(c5, 10, y, B12, FG);
        lPass[i][1] = text(c5, 80, y, F12, MUT);
        lPass[i][2] = textR(c5, hw - 76, y, 66, B12, FG);
    }

    // derecha: DX cluster
    lv_obj_t* c6 = card(scr, RX, TOP, RW, BOT - TOP, "DX CLUSTER", &lSpotNote);
    text(c6, 12, 28, F12, MUT, "MHz"); text(c6, 76, 28, F12, MUT, "DX"); text(c6, 166, 28, F12, MUT, "Mode");
    textR(c6, RW - 52, 28, 40, F12, MUT, "UTC");
    for (int i = 0; i < N_ROWS; i++) {
        const int ro = (ROW_H - 30) / 2;
        lv_obj_t* r = rowBox[i] = fill(c6, 3, 46 + i * ROW_H, RW - 8, ROW_H, SEL, 5);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(r, C(GREEN), 0);
        lv_obj_set_clickable(r, true);
        lv_obj_add_event_cb(r, rowClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        rowBar[i] = fill(r, 2, ro + 4, 3, 22, MUT);
        lRow[i][0] = text(r, 9, ro + 7, B12, FG);
        lRow[i][1] = text(r, 73, ro, B14, FG);
        lRow[i][2] = text(r, 73, ro + 15, F12, MUT);
        lRow[i][3] = text(r, 163, ro + 7, B12, MUT);
        lRow[i][4] = textR(r, RW - 8 - 48, ro + 7, 40, F12, MUT);
        lv_obj_set_width(lRow[i][1], 88); lv_label_set_long_mode(lRow[i][1], LV_LABEL_LONG_MODE_CLIP); lv_obj_set_height(lRow[i][1], 16);
        lv_obj_set_width(lRow[i][2], 88); lv_label_set_long_mode(lRow[i][2], LV_LABEL_LONG_MODE_CLIP); lv_obj_set_height(lRow[i][2], 15);
        lv_obj_set_hidden(r, true);
    }

    // pie: noticias DX
    fill(scr, 0, FOOT, W, 24, FIELD);
    fill(scr, 0, FOOT, W, 1, BORDER);
    lv_obj_t* tag = fill(scr, 0, FOOT + 1, 78, 23, ACC);
    lv_obj_t* tl = text(tag, 0, 0, B12, BG, "DX NEWS");
    lv_obj_center(tl);
    lNews = text(scr, 88, FOOT + 5, F14, FG, "");
    lv_obj_set_width(lNews, 730);
    lv_label_set_long_mode(lNews, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lSrv = textR(scr, W - 8 - 190, FOOT + 6, 190, F12, MUT);

    refreshSolar(); refreshSpots(); refreshActs(); refreshLink(); refreshRig();
    if (!cfg.ssid[0]) openSettings();
}

void update() {
    if (!M) return;
    static uint32_t rSolar = ~0u, rSpots = ~0u, rActs = ~0u, rPasses = ~0u, rNews = ~0u, rLink = ~0u, rRig = ~0u;
    static time_t lastSec = 0, lastMin = -1;
    time_t now = time(nullptr);
    bool timeOk = now > 1700000000;                    // sin hora (NTP) no hay reloj, noche ni pases
    bool minute = false;
    if (now != lastSec) {
        lastSec = now;
        char b[40];
        struct tm tm;
        if (timeOk) {
            gmtime_r(&now, &tm);
            snprintf(b, sizeof(b), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
            setText(lClock, b);
            strftime(b, sizeof(b), "%a %d %b %Y", &tm);
            setText(lDate, b);
            localtime_r(&now, &tm);
            snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
            setText(lLocal, b);
        } else {
            setText(lClock, "--:--:--"); setText(lDate, "waiting for time"); setText(lLocal, "--:--");
        }
        if (now / 60 != lastMin) { lastMin = now / 60; minute = true; }
    }
    if (rSolar != M->revSolar) { rSolar = M->revSolar; refreshSolar(); }
    if (rSpots != M->revSpots) { rSpots = M->revSpots; refreshSpots(); }
    if (rActs != M->revActs) { rActs = M->revActs; refreshActs(); }
    if (rPasses != M->revPasses || minute) { rPasses = M->revPasses; refreshPasses(now); }
    if (rNews != M->revNews) { rNews = M->revNews; setText(lNews, M->news[0] ? M->news : "No news yet"); }
    if (rLink != M->revLink) { rLink = M->revLink; refreshLink(); }
    if (rRig != M->revRig) { rRig = M->revRig; refreshRig(); }
    if (tuneFlash && (int32_t)(lv_tick_get() - tuneFlash) > 0) { tuneFlash = 0; setText(lTune, "TUNE"); lv_obj_center(lTune); }
    if (dxDirty || minute) refreshDeDx(now, timeOk);
    if (mapDirty || minute) drawMap(timeOk ? now : 0);
}

}

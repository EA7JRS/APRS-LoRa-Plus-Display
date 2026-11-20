# APRS LoRa+ Display — user manual

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later. Version **5.4.7**.

APRS LoRa+ Display is a touch screen for the **APRS LoRa+** tracker. It has no LoRa radio and no GPS: it links to the tracker
over Bluetooth LE, shows what the tracker hears and controls all of it, like the Android app. With Wi-Fi it adds the boat
network, internet weather, official alerts, the DX Cluster, the map and the rotor box.

Quick start: [QUICKSTART_EN.md](QUICKSTART_EN.md). Pins and what is still unchecked on each board: [HARDWARE.md](HARDWARE.md)
(Spanish). OpenHamClock clock: [HAMCLOCK.md](HAMCLOCK.md) (Spanish).

## 1. Boards and firmware

| Firmware (environment) | Waveshare board | Interface |
|---|---|---|
| `waveshare_lcd7b` | ESP32-S3-Touch-LCD-7B (1024×600) | Tabs |
| `waveshare_lcd7b_v2` | ESP32-S3-Touch-LCD-7B | Radio style (the app's) |
| `waveshare_lcd7b_hamclock` | ESP32-S3-Touch-LCD-7B | OpenHamClock clock |
| `waveshare_lcd7` | ESP32-S3-Touch-LCD-7, no B, rev 1.2 (800×480) | Tabs |
| `waveshare_lcd7_v2` | ESP32-S3-Touch-LCD-7, no B | Radio style, scaled |
| `waveshare_lcd35` | ESP32-S3-Touch-LCD-3.5 and 3.5-C (480×320) | Tabs |
| `waveshare_lcd28b` | ESP32-S3-Touch-LCD-2.8B (480×640) | Tabs |
| `waveshare_lcd169` | ESP32-S3-Touch-LCD-1.69 (240×280) | Tabs (icons) |

**The 7 and the 7B are different boards**: the 7 (800×480, CH422G expander) does not start with the 7B firmware (the screen
stays black) and the other way round. Check the silkscreen: the 7B says "7B".

Status: the LCD-7 with tabs has been tried on the board (screen, picture and touch). The rest builds, passes its tests on the
PC and is **untested on the board**.

## 2. Install

1. Open **https://ea7jrs.github.io/APRS-LoRa-Plus/install/#display** in Chrome or Edge (computer, or Android with an OTG cable).
2. Plug the board in over USB, press *Detect board* or pick it by hand, and choose the firmware from the table above.
3. *Install*. The settings stored on the board are kept.

On the 7B and the 7 the serial port is the UART USB-C (CH343): use that connector. Over USB the board answers `APRS-INFO?`
with `device=display`, which is how the installer recognises it.

You can also build with PlatformIO: `pio run -e <environment> -t upload`.

## 3. Link it to the tracker

1. On the tracker (WebUI > Bluetooth): Bluetooth on and, better, your own *Control PIN* (default `123456`).
2. On the display: *Settings* (tabs) or *Settings > TRACKER LINK* (v2): the same PIN. The address can stay empty: it links to
   the first tracker that advertises the KISS, TNC2 or control service. With several trackers around, type its BLE address.
3. The first time it pairs with the PIN; after that it links again by itself at start-up or when the link drops.
4. If you change the tracker PIN: *Forget pairing* and link again.

The tracker takes one control device at a time over BLE: while the phone app is linked, the display waits.

## 4. Tab interface (7B, LCD-7, 3.5, 2.8B, 1.69)

On the large screens the tabs have text on the left; on the small ones they are icons and tables have fewer columns.

| Tab | What it does |
|---|---|
| **Status** | GNSS position, radio mode, profile, battery. Beacon now, digipeater and mode change (the tracker restarts) |
| **Heard** | Stations heard by the tracker: distance from its GNSS, RSSI, SNR, age and comment |
| **Messages** | The tracker's APRS messages and sending with the touch keyboard |
| **Sensors** | Battery, temperature, humidity, pressure, QNH, barometric altitude, heading; wind with Beaufort; *Official alerts* and *Space weather* cards |
| **SAT / Rotor** | Current satellite (azimuth, elevation, range) and the tracker's rotor target |
| **Mirror** | Mirror of the tracker screen |
| **DAPNET** | Pages received by the tracker in DAPNET mode |
| **Network** | Boat network, internet and alerts (section 6) |
| **Settings** | Tracker PIN and address, forget pairing, backlight, palette (*Theme*: the app's or the rotor's, applied after a restart) and *Link > Clock* (UTC time and where it comes from) |

## 5. Radio-style interface (7B v2 and LCD-7 v2)

The same way of working as the Android app: a two-band radio screen, touch keys at the bottom and the **[F]** key.

- **RADIO screen**: band A = the tracker (mode, frequency, profile, S-meter of the last station heard, TX / RX / MSG / EMG
  LEDs); band B = the rotor box CAT. Below, BEACON, DIGI, ROTOR and the last stations heard.
- **[F]**: three pages of functions. *APRS*: S.LIST, MSG, BEACON, BCN (beacon now), GPS, WX, MAP, DISP, DAPNET, DIGI,
  WARNINGS. *RADIO*: DX, SAT, ROTOR, AIS, CW RX, MODE. *SYSTEM*: REMOTE, CONFIG, FIRMWARE, SENSORS, SETTINGS, TRACKER,
  REBOOT, ABOUT. Each key shows its state underneath (stations heard, satellite, ships...).
- **Touch keys**: the five at the bottom are chosen by holding down a key of the [F] menu (*Settings > DEFAULT TOUCH KEYS*
  puts back the defaults).
- **Side menu** (☰ button in the header): every screen.

| Screen | What it does |
|---|---|
| **Stations** / **Station** | List of stations heard and a card with a compass (north up or heading up), distance, bearing, signal, APRS weather and ROTOR |
| **Messages** / **Conversation** | Messages by conversation, write and answer |
| **Map** | Map with stations, AIS ships, your position and the rotor beam (section 7) |
| **Weather** | APRS weather stations with wind, Beaufort, dew point, feels-like and pressure trend |
| **Weather warnings** | Official alerts (section 8) |
| **Display** | Compass, GPS sky, altitude or clock (sun and moon) on the whole screen |
| **DX cluster** | Spots from a telnet node (EA4URE by default), with source, band and mode filters, and TUNE to the rotor box CAT |
| **Satellites** | Current pass, list and choice of satellite, with the rotor dial |
| **Rotor box** | APRS LoRa+ Rotor box over Wi-Fi (TCP 4534) or BLE: dial, go to, move, stop, park, follow the tracker or a station, CAT with Doppler |
| **AIS** | Ships sent by the tracker (AIS mode of the T-TWR) with CPA |
| **Tracker**, **Sensors**, **Remote**, **Config**, **Firmware** | As in the app: mode, profiles, sensors, every menu option of the tracker, its whole configuration and its firmware update over BLE |
| **DAPNET** | DAPNET pages |
| **Settings** | Tracker, Wi-Fi, DX Cluster, OpenWeatherMap, display, map, units (metric, nautical, imperial) |

**Rotor dial** (ROTOR and SAT, the same as in the app, the rotor box and sat-aprs): blue needle = position, amber = target,
green = tracker heading when it moves faster than 3 km/h, red = mechanical stops.

**Tracker firmware** (Firmware screen): with Wi-Fi, the display downloads the published version for the tracker's board,
checks its SHA-256 and sends it over Bluetooth; the tracker checks it again before activating it.

## 6. Local network and internet (Network tab on the tab boards)

The same connectivity as the plotter: Wi-Fi client, **one TCP server** and **UDP** listening, in a task of their own (if the
network drops, the screen goes on).

| Server | Port | What comes in |
|---|---|---|
| NMEA 0183 | 10110 | NMEA and AIS sentences from any device on the network |
| gpsd | 2947 | NMEA of the gpsd receiver |
| Signal K | 8375 | Own-boat data; notifications in *alarm* / *emergency* state |
| Empty address | 10110 | The phone giving the Wi-Fi, with the APRS LoRa+ app and its NMEA server on |

- **UDP** brings the NMEA broadcast on the network, such as the 4″ wind instrument's.
- **OpenWeatherMap** (free key): internet wind, pressure and temperature at the position, in amber (estimated).
- Each value comes from the best up-to-date source: network (LAN), tracker (TRK, position, course and speed of its GNSS) and
  internet (WEB). After 10 s without the best one, the next takes over.
- AIS targets of the network with range, bearing and CPA; red with CPA ≤ 0.5 nm and TCPA ≤ 20 min.
- **Calculations** (the same across APRS LoRa+): Beaufort, dew point, feels-like, the day's sun and moon, and true heading
  with the WMM2025 declination when only HDG / HDM arrive.
- **Time**: from the tracker GNSS or the network GPS; without GPS, NTP (`ntp.roa.es`, with `hora.roa.es` and `pool.ntp.org`).
  GPS time rules for 6 h.

On the v2 the Wi-Fi is set in *Settings > WIFI* and is used by the DX Cluster, internet weather (also sent to the tracker and,
the wind, to the rotor box), the map, the alerts and the firmware.

## 7. Map (v2)

Layers: OpenStreetMap, IGN topographic, PNOA orthophoto, dark, Esri (with your ArcGIS key) and radar; OpenSeaMap on top as
an option. Needs Wi-Fi. With a microSD the tiles you have seen are kept in `/maps` and work offline.
Data: © OpenStreetMap contributors; IGN Spain (CC BY 4.0); OpenSeaMap; Esri.

## 8. Official alerts

- **Tab boards** (with Wi-Fi): AEMET in Spain, MeteoAlarm in the rest of Europe (country in *Country (ISO)*), NWS in the US,
  tsunamis, earthquakes (USGS, EMSC, Raspberry Shake ShakeNet) and space weather (SWPC, with the NOAA scale colours: yellow, light orange, orange, red and dark red for levels 1 to 5), every 15 min. AEMET's, with a position (from the tracker or
  the network), are those whose polygon has that position inside; without a position, by the words in *Alert zones*
  ("Huelva, Cádiz"). Pop-up window in the colour of the level and a summary in Sensors.
- **v2**: *Weather warnings* screen (WARN key): AEMET (with your free OpenData key, with polygon and distance) or Meteoalarm
  (country and zone); *POP-UP WINDOW FROM* chooses from which level the window opens (yellow, orange, red or never).
- **All**: the alerts sent by the tracker (the app downloads them: AEMET, MeteoAlarm, NWS, earthquakes, tsunamis, SAME), with
  the space weather.

## 9. Emergency alerts

A centred window over any screen, with the plotter's and the app's design: APRS emergency beacon or message, AIS danger
(DISTRESS, collision risk) and the tracker alarm (EAI, man overboard). SILENCE ALARM, MAP, STATION and CLOSE. The same station
does not open it again for 10 min. The boards have no buzzer.

## 10. Troubleshooting

| What happens | What to check |
|---|---|
| Black screen | Firmware for the right board? (7 against 7B). Serial console at 115200: the start-up log says what fails |
| Does not link to the tracker | Tracker Bluetooth on, same PIN, phone app disconnected; *Forget pairing* after changing the PIN |
| No network data | Wi-Fi and server in Network; the state is at the top of the tab |
| Alerts or map empty | Wi-Fi with internet access and the time set (Settings > Link > Clock) |

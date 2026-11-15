# APRS LoRa+ Display — quick start

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later. Version **5.4.6**. Full manual: [MANUAL_EN.md](MANUAL_EN.md).

## 1. Choose the firmware

| Your Waveshare board | Firmware |
|---|---|
| ESP32-S3-Touch-LCD-**7B** | `waveshare_lcd7b` (tabs) or `waveshare_lcd7b_v2` (like the app) or `waveshare_lcd7b_hamclock` (clock) |
| ESP32-S3-Touch-LCD-**7** (no B) | `waveshare_lcd7` (tabs) or `waveshare_lcd7_v2` (like the app) |
| ESP32-S3-Touch-LCD-3.5 / 3.5-C | `waveshare_lcd35` |
| ESP32-S3-Touch-LCD-2.8B | `waveshare_lcd28b` |
| ESP32-S3-Touch-LCD-1.69 | `waveshare_lcd169` |

The 7 and the 7B do not mix: with the other one's firmware, black screen.

## 2. Install

1. Chrome or Edge: **https://ea7jrs.github.io/APRS-LoRa-Plus/install/#display**.
2. Board over USB (on the 7 and 7B, the UART USB-C) → *Detect board* → firmware → *Install*.

## 3. Link it to the tracker

1. Tracker: WebUI > Bluetooth on; note the *Control PIN* (default `123456`).
2. Display: *Settings* (on the v2, *Settings > TRACKER LINK*) → the PIN → save. It pairs and links again by itself.
3. Disconnect the phone app: the tracker takes one control device at a time.

## 4. Wi-Fi (optional)

- Tabs: **Network** tab → network, password and server (NMEA 10110, gpsd 2947, Signal K 8375, or empty for the phone).
- v2: *Settings > WIFI*. With it: DX Cluster, OpenWeatherMap weather (free key), map, alerts and tracker firmware.

## 5. Everyday use

| To | Tabs | v2 |
|---|---|---|
| See stations | Heard | S.LIST |
| Messages | Messages | MSG |
| Beacon now | Status | BCN |
| Change mode | Status | [F] > MODE |
| Rotor | SAT / Rotor | ROTOR |
| Official alerts | Sensors | [F] > WARNINGS |
| Map | — | MAP |

Rotor dial: blue = position, amber = target, green = tracker heading when moving, red = stops.

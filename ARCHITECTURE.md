# Architecture Overview

## Boot Sequence

1. **Hardware Init** — Serial, EEPROM (with in-RAM cache), buzzer, LED, optional NeoPixel
2. **WiFi** — Connect to saved network (STA mode) or start AP with captive portal
3. **Time Sync** — NTP sync with configured timezone
4. **Scheduler** — Fetch Shabbat times from Hebcal, build alert events
5. **Web Server** — Start HTTP server on port 80

## Main Loop

Each `loop()` iteration handles:

- WiFi management and reconnection
- Web server requests (dashboard, settings, API)
- NTP time maintenance (hourly re-sync)
- Schedule refresh (daily, Thursday night, Friday morning)
- Alert checking (compare current time vs scheduled events)
- RTTTL audio update (non-blocking ringtone playback)
- Twice-daily safety reboot (00:00 and 12:00 local) — only if WiFi up, time synced, no alert playing, Shabbat not active

## Data Flow

```
┌─────────────┐
│  Hebcal API │  HTTP GET /shabbat?cfg=json&lat=...&lon=...
│  (port 80)  │  (plain HTTP — no TLS handshake on the ESP8266 heap)
└──────┬──────┘
       │
       ▼
┌──────────────┐
│ HebcalClient │  Parse JSON stream → extract candle/havdalah timestamps
└──────┬───────┘
       │  ShabbatEvent[] cached in Scheduler
       ▼
┌──────────────┐
│  Scheduler   │  Build AlertEvent[] based on user's checkbox selections
│              │  (18 min, 30 min, 45 min before candles + havdalah)
└──────┬───────┘
       │  Events sorted by time, checked each loop
       ▼
┌──────────────┐
│   Alerts     │  Trigger buzzer/speaker (RTTTL) + LED when event time reached
└──────────────┘
```

## Alert Event Types

The user selects which candle lighting alerts to receive (checkboxes in Settings):

| Type        | Description                          |
| ----------- | ------------------------------------ |
| `candles-45` | 45 minutes before candle lighting   |
| `candles-30` | 30 minutes before candle lighting   |
| `candles-18` | 18 minutes before candle lighting   |
| `havdalah`   | At havdalah time                    |

Selections are stored as a bitmask (bit 0 = 18 min, bit 1 = 30 min, bit 2 = 45 min).
Changing checkboxes instantly rebuilds the event list from cached data (no network call).

## Storage Layout (EEPROM, ESP8266)

Fixed addresses starting at offset 0. All values are mirrored to a `StorageCache`
struct in RAM populated on boot, so getters never re-read EEPROM (avoids the
heap fragmentation that comes with allocating a fresh `String` per read).

| Field                  | Description                                     |
| ---------------------- | ----------------------------------------------- |
| `wifi_ssid`            | WiFi network name                               |
| `wifi_pass`            | WiFi password                                   |
| `city_name`            | Location city                                   |
| `latitude`/`longitude` | Coordinates                                     |
| `timezone`             | POSIX timezone string                           |
| `candle_offset`        | Minutes before sunset for candle-lighting time  |
| `candle_alerts`        | Bitmask: which candle alerts are enabled        |
| `havdalah_mode`        | "M" (degrees), "m" (minutes), or "degrees"      |
| `havdalah_minutes`     | Minutes after sunset (when mode = "m")          |
| `havdalah_degrees`     | Degrees below horizon (when mode = "degrees")   |
| `ringtone`             | Selected RTTTL ringtone id                      |
| `alert_duration_ms`    | How long alert plays                            |
| `last_schedule`        | Cached schedule string                          |
| `hebcal_max_attempts`  | Max retry count for Hebcal API                  |
| `hebcal_proxy_url`     | Optional HTTP/HTTPS proxy                       |

## Web Server Endpoints

| Route             | Description                                |
| ----------------- | ------------------------------------------ |
| `/`               | Status dashboard with countdown            |
| `/setup`          | First-time configuration form              |
| `/settings`       | Configuration form                         |
| `/logs`           | Pointer to serial console                  |
| `/api/status`     | JSON: time, location, Shabbat state        |
| `/api/schedule`   | JSON: upcoming events with times           |
| `/api/settings`   | POST: update configuration                 |
| `/api/test-alert` | POST: trigger test sound                   |
| `/api/geocode`    | POST: look up city -> {lat, lon, timezone} |
| `/api/rgb-preview`| POST: toggle Shabbat NeoPixel preview      |

The HTML pages (dashboard, settings, setup, logs) are stored as PROGMEM
constants and streamed with `server.send_P()` to avoid building a multi-KB
`String` on the heap per request. JSON responses are serialized into a fixed
local buffer and pushed via `server.sendContent()`.

## Memory Budget (ESP8266)

| Section | Approx. usage |
| ------- | ------------- |
| Flash   | ~530 KB out of 1 MB |
| Static RAM | ~37 KB out of 80 KB |
| Free heap (idle) | ~25 KB |

Heap-fragmentation watchdog: with `DEBUG_HEAP=1` (in `config.h`) the firmware
prints `freeHeap`/`maxFreeBlock` every 5 minutes on Serial. The twice-daily
safety reboot in `main.cpp` keeps the device healthy even under sustained
fragmentation.

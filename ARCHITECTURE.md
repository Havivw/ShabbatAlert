# Architecture Overview

## Boot Sequence

1. **Hardware Init** — Serial, storage (NVS/EEPROM), buzzer, LED
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

## Data Flow

```
┌─────────────┐
│  Hebcal API │  HTTP GET /shabbat?cfg=json&lat=...&lon=...
│  (port 80)  │  (ESP8266: plain HTTP, ESP32: HTTPS)
└──────┬──────┘
       │
       ▼
┌──────────────┐
│ HebcalClient │  Parse JSON → extract candle/havdalah timestamps
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
│   Alerts     │  Trigger buzzer/speaker + LED when event time reached
└──────────────┘
```

## Alert Event Types

The user selects which candle lighting alerts to receive (checkboxes in Settings):

| Type | Description |
|------|-------------|
| `candles-45` | 45 minutes before candle lighting |
| `candles-30` | 30 minutes before candle lighting |
| `candles-18` | 18 minutes before candle lighting |
| `havdalah` | At havdalah time |

Selections are stored as a bitmask (bit 0 = 18 min, bit 1 = 30 min, bit 2 = 45 min).
Changing checkboxes instantly rebuilds the event list from cached data (no network call).

## Storage Layout

### ESP32 (NVS/Preferences)
Key-value store under namespace `"shabbat"`.

### ESP8266 (EEPROM)
Fixed addresses starting at offset 0. Key fields:

| Field | Description |
|-------|-------------|
| `wifi_ssid` | WiFi network name |
| `wifi_pass` | WiFi password |
| `city_name` | Location city |
| `latitude` / `longitude` | Coordinates |
| `timezone` | POSIX timezone string |
| `candle_alerts` | Bitmask: which candle alerts are enabled |
| `havdalah_mode` | "M" (degrees) or "m" (minutes) |
| `ringtone` | Selected RTTTL ringtone name |
| `alert_duration_ms` | How long alert plays |
| `last_schedule` | Cached schedule JSON |

## Web Server Endpoints

| Route | Handler | Description |
|-------|---------|-------------|
| `/` | `getDashboardHTML` | Status dashboard with countdown |
| `/settings` | `getSettingsHTML` | Configuration form |
| `/logs` | `getLogsHTML` | System log viewer |
| `/api/status` | `handleAPIStatus` | JSON: time, location, Shabbat state |
| `/api/schedule` | `handleAPISchedule` | JSON: upcoming events with times |
| `/api/settings` | `handleAPISettings` | POST: update configuration |
| `/api/test-alert` | `handleTestAlert` | POST: trigger test sound |

## Memory

| Platform | Flash | Free RAM | Notes |
|----------|-------|----------|-------|
| ESP32 | ~1.2 MB | ~200 KB | Full features, OLED optional |
| ESP8266 | ~520 KB | ~40 KB | RTTTL audio, no OLED |

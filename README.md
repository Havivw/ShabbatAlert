# Shabbat Alert Device

An ESP8266 firmware that automatically alerts you for Shabbat candle lighting and havdalah times, with a web-based configuration UI.

## Features

- **Automatic Shabbat Scheduling**: Fetches candle lighting and havdalah times from the Hebcal API
- **Configurable Candle Alerts**: Choose any combination of 18, 30, or 45 minutes before candle lighting
- **Havdalah Alert**: Alert at havdalah time (configurable: degrees below horizon or fixed minutes)
- **Web UI**: Responsive dashboard and settings page accessible via browser
- **WiFi Configuration**: Captive portal for easy initial setup
- **RTTTL Ringtones**: Multiple selectable ringtones for alerts (Pinky, Star Wars, Mozart, Under the Sea, Spiderman, Mario, Pink Panther, Hava Nagila)
- **Location Support**: City search with automatic coordinates lookup
- **Hardware Outputs**: Buzzer/speaker, LED, optional WS2812 NeoPixel ring
- **REST API**: JSON endpoints for status, schedule, and settings
- **Persistent Storage**: Settings and schedule cached across reboots
- **Memory hardening**: PROGMEM HTML, cached settings, twice-daily safety reboot

## Hardware Requirements

### ESP8266 NodeMCU v2 (or compatible)

- ESP8266 NodeMCU v2 / Wemos D1 Mini / similar
- Speaker module on **D8 (GPIO 15)** for RTTTL audio (passive speaker via I2S no-DAC)
- LED on **D6 (GPIO 12)** via 220 ohm resistor (status / alert blink)
- Optional: physical reset button on **D5 (GPIO 14)** to GND
- Optional: WS2812 NeoPixel ring on **D2 (GPIO 4)** for Shabbat indicator

### Wiring

```
ESP8266        Component
-------        ---------
D8 (GPIO15) -> Speaker / Buzzer signal
D6 (GPIO12) -> LED (+) via 220 ohm resistor
D5 (GPIO14) -> Reset button (to GND, optional)
D2 (GPIO4)  -> NeoPixel DIN (optional)
GND         -> Speaker GND, LED (-), Button GND, NeoPixel GND
5V          -> Speaker VCC, NeoPixel VCC
```

## Installation

### Prerequisites

1. **PlatformIO**: Install [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) or CLI
2. **USB Driver**: CH340, CP2102, or FTDI driver for your board

### Build and Flash

```bash
cd shabbat_alert

# Build
pio run

# Upload to device (auto-detects port; pass --upload-port if needed)
pio run -t upload

# Monitor serial output
pio device monitor -b 115200
```

### First-Time Setup

1. Power on the device — it starts in Access Point mode
2. Connect to WiFi network `ShabbatAlert-XXXX` (password: `shabbat123`)
3. Open browser at `http://192.168.4.1`
4. Enter your WiFi credentials, city name, and preferences
5. Save — device restarts and connects to your WiFi

### After Setup

Access the device in your browser at:

> **http://shabbatalert.local/**

(If mDNS is not supported on your network, use the device IP shown in the serial monitor, e.g. `http://192.168.1.xxx`)

## Web Interface

- **Dashboard**: [http://shabbatalert.local/](http://shabbatalert.local/) — Current time, Shabbat status, upcoming alerts with date/time and countdown
- **Settings**: [http://shabbatalert.local/settings](http://shabbatalert.local/settings) — WiFi, location, alert checkboxes (18/30/45 min), havdalah mode, ringtone
- **Logs**: [http://shabbatalert.local/logs](http://shabbatalert.local/logs) — Pointer to serial console for debugging

## Alert Configuration

### Candle Lighting Alerts

In Settings, check any combination of:

- **18 minutes** before candle lighting
- **30 minutes** before candle lighting
- **45 minutes** before candle lighting

### Havdalah

- **Nightfall** (degrees below horizon, default 8.5)
- **Fixed minutes** after sunset (42, 50, or 72 minutes)

## REST API

| Endpoint          | Method | Description                                |
| ----------------- | ------ | ------------------------------------------ |
| `/api/status`     | GET    | Device status, times, Shabbat state        |
| `/api/schedule`   | GET    | Upcoming alert events with countdown       |
| `/api/settings`   | POST   | Update configuration (JSON body)           |
| `/api/test-alert` | POST   | Trigger a test alert                       |
| `/api/geocode`    | POST   | Look up city coordinates and timezone      |
| `/api/rgb-preview`| POST   | Toggle Shabbat NeoPixel preview            |

## Project Structure

```
src/
├── main.cpp            # Main firmware loop (incl. twice-daily safety reboot)
├── config.h            # Pin definitions and constants
├── storage.h/cpp       # Persistent settings (EEPROM with in-RAM cache)
├── wifi_manager.h/cpp  # WiFi AP/STA and captive portal
├── time_sync.h/cpp     # NTP time synchronization
├── hebcal_client.h/cpp # Hebcal API integration (plain HTTP)
├── scheduler.h/cpp     # Alert event scheduling
├── alerts.h/cpp        # Buzzer and LED output
├── rtttl_audio.h/cpp   # RTTTL ringtone playback
├── geocoding.h/cpp     # City name to coordinates lookup
├── logger.h/cpp        # Serial logging helpers
└── ui_server.h/cpp     # Web server (HTML in PROGMEM, JSON streamed)
```

## Technical Notes

- Plain HTTP is used to talk to Hebcal (port 80) — no TLS handshake on the constrained ESP8266 heap.
- Schedule is cached locally in EEPROM and refreshes daily (plus Thursday night and Friday morning).
- The web server remains responsive during Hebcal API fetches via an idle callback mechanism.
- HTML pages are stored in flash (`PROGMEM`) and streamed with `send_P()` to avoid large heap allocations.
- All settings are mirrored to a small RAM cache so getters never re-read EEPROM.
- The device performs a safety reboot twice a day (00:00 and 12:00 local) — only when WiFi is up, time is synced, no alert is playing and Shabbat is not currently active — to release any heap fragmentation.

## Troubleshooting

- **Device won't connect to WiFi**: Check 2.4 GHz network, verify credentials in serial monitor
- **Time not syncing**: Verify WiFi is connected; NTP requires internet access
- **No alerts**: Check dashboard for schedule; verify checkboxes are set in Settings
- **Web UI not loading**: Find IP in serial monitor; try `http://shabbatalert.local`

## Credits

- [Hebcal API](https://www.hebcal.com) — Shabbat times and Jewish calendar data
- [PlatformIO](https://platformio.org) — Build system
- [ArduinoJson](https://arduinojson.org) — JSON parsing
- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) — RTTTL audio playback
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) — WS2812 driver

## License

This project is provided as-is for personal and educational use.

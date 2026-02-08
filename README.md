# Shabbat Alert Device

An ESP32/ESP8266 firmware that automatically alerts you for Shabbat candle lighting and havdalah times, with a web-based configuration UI.

## Features

- **Automatic Shabbat Scheduling**: Fetches candle lighting and havdalah times from the Hebcal API
- **Configurable Candle Alerts**: Choose any combination of 18, 30, or 45 minutes before candle lighting
- **Havdalah Alert**: Alert at havdalah time (configurable: 8 degrees below horizon or fixed minutes)
- **Web UI**: Responsive dashboard and settings page accessible via browser
- **WiFi Configuration**: Captive portal for easy initial setup
- **RTTTL Ringtones**: Multiple selectable ringtones for alerts (ESP8266)
- **Location Support**: City search with automatic coordinates lookup
- **Hardware Outputs**: Buzzer/speaker, LED, and optional OLED display
- **REST API**: JSON endpoints for status, schedule, and settings
- **Persistent Storage**: Settings and schedule cached across reboots

## Hardware Requirements

### ESP32-WROOM
- ESP32 development board
- Buzzer (passive or active) on GPIO 25
- LED on GPIO 26 (via 220 ohm resistor)
- Optional: SSD1306 OLED display (I2C on GPIO 21/22)

### ESP8266 NodeMCU
- ESP8266 NodeMCU v2 or similar
- Speaker module on D8 (GPIO 15) for RTTTL audio
- LED on D6 (GPIO 12) via 220 ohm resistor
- Optional: Physical reset button on D5 (GPIO 14)

## Wiring (ESP8266 NodeMCU)

```
ESP8266        Component
-------        ---------
D8 (GPIO15) -> Speaker / Buzzer signal
D6 (GPIO12) -> LED (+) via 220 ohm resistor
D5 (GPIO14) -> Reset button (to GND, optional)
GND         -> Speaker GND, LED (-), Button GND
5V          -> Speaker VCC (if powered module)
```

## Installation

### Prerequisites

1. **PlatformIO**: Install [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) or CLI
2. **USB Driver**: CH340, CP2102, or FTDI driver for your board

### Build and Flash

```bash
cd shabbat_alert

# Build for ESP8266
pio run -e esp8266mod

# Upload to device
pio run -e esp8266mod -t upload

# Monitor serial output
pio device monitor
```

For ESP32:
```bash
pio run -e esp32-wroom -t upload
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
- **Logs**: [http://shabbatalert.local/logs](http://shabbatalert.local/logs) — System logs for debugging

## Alert Configuration

### Candle Lighting Alerts
In Settings, check any combination of:
- **18 minutes** before candle lighting
- **30 minutes** before candle lighting
- **45 minutes** before candle lighting

### Havdalah
- **Nightfall** (degrees below horizon, default 8)
- **Fixed minutes** after sunset (42, 50, or 72 minutes)

## REST API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Device status, times, Shabbat state |
| `/api/schedule` | GET | Upcoming alert events with countdown |
| `/api/settings` | POST | Update configuration (JSON body) |
| `/api/test-alert` | POST | Trigger a test alert |

## Project Structure

```
src/
├── main.cpp            # Main firmware loop
├── config.h            # Pin definitions and constants
├── storage.h/cpp       # Persistent settings (NVS / EEPROM)
├── wifi_manager.h/cpp  # WiFi AP/STA and captive portal
├── time_sync.h/cpp     # NTP time synchronization
├── hebcal_client.h/cpp # Hebcal API integration (plain HTTP on ESP8266)
├── scheduler.h/cpp     # Alert event scheduling
├── alerts.h/cpp        # Buzzer and LED output
├── rtttl_audio.h/cpp   # RTTTL ringtone playback (ESP8266)
├── geocoding.h/cpp     # City name to coordinates lookup
├── logger.h/cpp        # Ring buffer logging
└── ui_server.h/cpp     # Web server, dashboard, settings UI
```

## Technical Notes

- **ESP8266 uses plain HTTP** to Hebcal (port 80) — no TLS needed. Hebcal serves JSON over HTTP without redirect.
- **ESP32 uses HTTPS** via mbedTLS (supports TLS 1.3).
- Schedule is cached locally and refreshes daily (plus Thursday night and Friday morning).
- The web server remains responsive during Hebcal API fetches via an idle callback mechanism.

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

## License

This project is provided as-is for personal and educational use.
# ShabbatAlert

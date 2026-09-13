# Sky Overhead

[![CI](https://github.com/kovaacs/sky_overhead/actions/workflows/ci.yml/badge.svg)](https://github.com/kovaacs/sky_overhead/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/kovaacs/sky_overhead)](https://github.com/kovaacs/sky_overhead/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

<p align="center">
  <img src="assets/sky-overhead-display.jpeg" width="640" alt="Sky Overhead running on a Seeed reTerminal E1001, displaying a nearby aircraft alongside temperature and humidity readings">
</p>

Sky Overhead turns a Seeed reTerminal E1001 into a quiet wall display for nearby aircraft. It shows the nearest aircraft's type, callsign, registration, airline, route, altitude, trend, and speed alongside indoor temperature and humidity.

The display wakes periodically, fetches current data, redraws when useful information changes, and returns to sleep. Temporary network failures leave the last good screen in place, and optional quiet hours pause aircraft checks overnight.

## Features

- Nearest-aircraft data from a public ADS-B service, with an optional local receiver fallback
- Callsign-based airline and route details
- Configurable units, search radius, refresh timing, and quiet hours
- QR link to more information about the displayed live or retained aircraft
- Indoor temperature and humidity from the device's built-in sensor
- E-paper-aware refresh behavior that limits unnecessary updates and ghosting

## Requirements

- Seeed reTerminal E1001 with XIAO ESP32S3
- FAT-formatted microSD card
- Wi-Fi network with internet access
- Observer latitude, longitude, altitude, and timezone

## Install

Download the merged firmware image and checksums from the [latest GitHub release](https://github.com/kovaacs/sky_overhead/releases/latest). Follow [FLASHING.md](FLASHING.md) to verify and install the image and prepare the microSD card.

## Configure

Copy [`config.example.txt`](config.example.txt) to `/config.txt` at the root of the microSD card and replace the placeholder values. Use one `KEY=VALUE` pair per line. Spaces around `=` are accepted, setting names and documented option values are case-insensitive, and blank lines or lines beginning with `#` are ignored.

Required settings:

- `SSID`: Wi-Fi network name
- `LAT`, `LON`: observer location in decimal degrees
- `ALT`: observer altitude in meters above sea level
- `TZ`: POSIX timezone string used for local timestamps and quiet hours

Optional settings:

- `PASS`: Wi-Fi password; leave empty for an open network
- `SPEED`: `kph`, `mph`, or `kts`
- `HEIGHT`: `ftfl` or `metric`
- `TEMP`: `c` or `f`
- `RADIUS`: aircraft search radius in kilometers, from 1 to 463
- `NIGHT_MODE`: quiet-hours range in `HH:MM-HH:MM`; omit or leave empty to disable
- `BUSY`: normal sleep interval in seconds, from 15 to 600
- `MAX_REFRESH`: time in seconds after which the next wake forces a display update; `0` disables forced updates, while positive values range from 60 to 86400
- `LOCAL_ADSB_URL`: optional readsb/tar1090 base URL, such as `http://192.168.1.20:8080`; the firmware appends `/data/aircraft.json`
- `QR_URL`: aircraft-information URL template containing `{reg}`; leave empty to hide the QR code

Defaults are provided for units, radius, and sleep interval. `QR_URL` defaults to `https://www.flightradar24.com/data/aircraft/{reg}`. The QR code is hidden when no registration is available or the generated URL is too long. Prefer a DHCP-reserved address over an `.local` hostname for a local ADS-B receiver.

Example timezone values:

- Central Europe: `CET-1CEST,M3.5.0,M10.5.0/3`
- UTC: `UTC0`

## Display Behavior

- The left side shows the nearest current aircraft, or the last-seen aircraft when no current aircraft is found.
- The right side shows indoor temperature and humidity.
- Quiet hours show a sleep screen and pause aircraft checks until morning.
- The footer shows the local refresh time and the sources used for the displayed data.
- Missing aircraft fields are omitted rather than leaving blank rows.
- A QR code links to information about the displayed live or retained aircraft when its registration is available.

The screen is intentionally not updated second by second. A different aircraft or other static display change causes a redraw, while telemetry and climate changes update with the next redraw. `MAX_REFRESH` can ensure periodic updates, but it does not shorten `BUSY` sleep intervals or interrupt quiet hours.

## Data Sources

- [adsb.lol](https://adsb.lol/) provides public live-aircraft data.
- An optional local readsb/tar1090 receiver is used when the public aircraft request fails.
- [adsb.im](https://adsb.im/) provides route information based on the aircraft callsign and position.

If the public source successfully reports an empty sky, the local receiver is not queried. Source labels in the display footer identify whether current or retained data is shown.

## Data and Privacy

Configuration is read from the microSD card. `config.txt` is ignored by Git to reduce the risk of publishing it accidentally, and the firmware does not send the Wi-Fi password to a data provider.

The configured observer latitude and longitude are included in requests to `adsb.lol`. The optional local ADS-B receiver is queried only after a failed public aircraft request. Aircraft position and callsign are sent to `adsb.im` for route lookup.

HTTPS certificate verification is disabled in the current firmware to accommodate the embedded networking stack. Do not treat aircraft or route data as authenticated or safety-critical information.

## Build and Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md) for source builds, uploads, debug options, tests, hardware notes, and contribution guidelines.

## License

Sky Overhead is available under the [MIT License](LICENSE).

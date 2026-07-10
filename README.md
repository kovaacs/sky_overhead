# Sky Overhead

Sky Overhead is an Arduino sketch for the Seeed reTerminal E1001 / XIAO ESP32S3. It shows the nearest overhead aircraft on the e-paper display, with type, callsign, tail number, airline, route, altitude, trend, and speed. A side panel shows the onboard temperature and humidity sensor.

It is built to behave like a quiet wall appliance: wake, fetch, redraw only when the visible data changes, then sleep. Temporary network failures leave the last good screen in place, and quiet hours pause aircraft checks overnight.

## Hardware

- Seeed reTerminal E1001 with XIAO ESP32S3
- Built-in 800 x 480 e-paper display, UC8179 driver, via Seeed_GFX / TFT_eSPI
- Built-in SHT4x temperature/humidity sensor on I2C, GPIO19 SDA / GPIO20 SCL
- microSD card for `/config.txt`, using HSPI: CS GPIO14, SCK GPIO7, MOSI GPIO9, MISO GPIO8
- SD card power-enable on GPIO16
- Battery voltage sense on GPIO1 after pulling GPIO21 high
- Serial debug on `Serial1`, GPIO43 TX / GPIO44 RX, 115200 baud; this is not USB CDC
- Wi-Fi network with internet access

## Data Sources

The sketch uses keyless public APIs:

- `adsb.lol` for live aircraft position, aircraft type, and registration
- `adsb.im` for route lookup by callsign plus live aircraft position

## Local Files

- `sky_overhead.ino`: main sketch and hardware orchestration
- `Aircraft.h`: aircraft data model and display formatting helpers
- `AdsbParser.h`, `RouteParser.h`, `JsonHelpers.h`: ADS-B and route JSON parsing
- `Climate.h`, `ClimateSensor.h`: temperature/humidity formatting and SHT4x retry logic
- `Config.h`: SD-card config parsing and runtime settings
- `DisplayView.h`, `DisplayRenderer.h`: display view models and rendering
- `RetainedState.h`: RTC-retained aircraft and signature state
- `TimeRules.h`: quiet-hours and wake scheduling rules
- `driver.h`: display board selection, currently `BOARD_SCREEN_COMBO 520`
- `IconFont.h`: generated bitmap icon font used by the display UI
- `tools/test_*.cpp`: host-side unit tests for pure parsing, formatting, state, config, display-view, and timing logic
- `tools/run_unit_tests.sh`: builds and runs the host-side unit test suite
- `tools/generate_icon_font.py`: regenerates `IconFont.h` from Lucide SVG sources
- `assets/icons/lucide/`: cached Lucide source SVGs and license

## Runtime Lifecycle

Each update is a full reboot from deep sleep. While the device sleeps, only the e-paper panel holds the previous image.

```text
DEEP SLEEP
  |
  | timer wake
  v
BOOT
  - start Serial1
  - read /config.txt from SD
  - initialize e-paper
  - initialize SHT4x
  |
  v
connect Wi-Fi
  |
  +-- Wi-Fi failed -> sleep 45 seconds
  |
  v
NTP sync
  |
  +-- quiet hours -> show night screen, sleep until morning
  |
  v
fetchOverhead()
  |
  +-- ADS-B request failed -> keep current screen, sleep 45 seconds
  |
fetchRoute() if an aircraft was found
batteryPct()
readClimate()
  |
  v
build visible-content signature
  |
  +-- same as rtcSig -> skip drawing and sleep
  |
  v
draw into RAM buffer
epaper.update()
save rtcSig
sleep BUSY seconds
```

State that must survive deep sleep lives in `RTC_DATA_ATTR`: the last rendered signature, last-seen aircraft, last route, timestamp, and redraw count. It is lost on full power loss.

## Display Behavior

- Left column: active aircraft, or retained last-seen aircraft when nothing current is found.
- Right column: indoor temperature and humidity.
- Quiet hours: moon-icon sleep screen while aircraft checks are paused.
- Footer: prominent local `Last refreshed` time.
- Primary aircraft label: type code such as `A321`; rotorcraft use a helicopter glyph when ADS-B reports category `A7`.
- Secondary labels: callsign and tail number, for example `FIN7EH (OH-LZH)`, then airline.
- Detail row: altitude, vertical trend, and speed, for example `FL132 | climbing | 307 kts`.
- Missing aircraft fields collapse upward instead of leaving blank rows.
- Unchanged visible state skips the e-paper refresh; every 20 redraws, a full white refresh reduces accumulated ghosting.

The screen is intentionally not live second-by-second. Each wake uses fresh data, but refreshes only when the visible state changes enough to justify an e-paper update. Drawing happens in the display RAM buffer first; the slow panel transfer happens during `epaper.update()`.

Reusable display glyphs are generated from Lucide SVGs. The generator downloads and caches missing source SVGs in `assets/icons/lucide/`. After adding or changing icons, install `rsvg-convert` and ImageMagick's `magick`, then run:

```bash
python3 tools/generate_icon_font.py
```

## Hardware Quirks

- Keep `SPIClass spiSD(HSPI)` local inside SD-card functions. A file-scope `SPIClass` can crash on boot because the constructor runs before FreeRTOS is ready.
- Avoid partial e-paper refresh. `updataPartial()` exists in the UC8179 driver, but it produces heavy ghosting with the built-in waveform LUT. Use full `update()` only.
- Use `460800` upload speed. `921600` can drop this USB-serial adapter during the baud switch.
- The SD card slot is explicitly powered only while reading config, then powered down before Wi-Fi and fetch work.

## Arduino Setup

Install Arduino CLI, the ESP32 board package, and the required libraries.

Add the Espressif package index if it is not already configured:

```bash
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Install libraries:

```bash
arduino-cli lib install ArduinoJson
arduino-cli lib install "Sensirion I2C SHT4x"
arduino-cli lib install "Sensirion Core"
```

Install Seeed_GFX into your Arduino libraries folder. This provides the reTerminal E Series e-paper `TFT_eSPI.h` / `EPaper` stack; it is not the stock Bodmer TFT_eSPI library.

```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/Seeed-Studio/Seeed_GFX.git
```

The sketch includes `driver.h` with:

```c
#define BOARD_SCREEN_COMBO 520
```

That selects Seeed's E1001 display setup (`Setup520_Seeed_reTerminal_E1001`). If compilation fails with missing `TFT_eSPI.h`, `EPaper`, or `EPAPER_ENABLE`, check Seeed_GFX and `driver.h`.

Seeed's reTerminal E Series Arduino cookbooks are useful references for the display and onboard peripherals:

- https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_display/
- https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/

## Board Options

Use the XIAO ESP32S3 target with these options:

```text
esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB
```

The `460800` upload speed is intentional. On this USB-serial adapter, `921600` can drop the connection during upload.

## SD Card Config

Runtime settings are read from a plain text file at the root of the microSD card:

```text
/config.txt
```

Use a FAT-formatted card and create `config.txt` in the root directory. Use one `KEY=VALUE` pair per line. Spaces around `=` are accepted; quoted values are not needed. Setting names are case-insensitive, as are documented option values such as `kts`, `metric`, `f`, `true`, and `on`. Blank lines and `#` comments are ignored.

Minimal working example:

```text
SSID=your-wifi-name
PASS=your-wifi-password
LAT=47.4979
LON=19.0402
ALT=100
TZ=CET-1CEST,M3.5.0,M10.5.0/3
SPEED=kph
HEIGHT=ftfl
TEMP=c
RADIUS=30
NIGHT_MODE=23:00-07:00
BUSY=60
DEMO=0
```

Required fields:

- `SSID`: Wi-Fi network name
- `PASS`: Wi-Fi password
- `LAT`, `LON`: observer location in decimal degrees
- `ALT`: observer altitude in meters
- `TZ`: POSIX timezone string used for local timestamps and quiet hours

Display and behavior fields:

- `SPEED`: `kph`, `mph`, or `kts`
- `HEIGHT`: `ftfl` or `metric`
- `TEMP`: `c` or `f`
- `RADIUS`: aircraft search radius in kilometers
- `NIGHT_MODE`: quiet-hours range in `HH:MM-HH:MM`; omit it or leave it empty to disable night mode
- `BUSY`: normal sleep interval in seconds
- `DEMO`: `1` to skip network fetches and cycle through dummy live, retained-aircraft, and night screens for layout iteration; `0` for normal operation

Units, radius, and sleep interval have defaults. Quiet hours are disabled unless `NIGHT_MODE` is configured. Wi-Fi and observer location must be valid for useful results. If the SD card or config file is missing, the device boots without Wi-Fi and retries after a short sleep.

Example timezone values:

- Budapest/Berlin/Central Europe: `CET-1CEST,M3.5.0,M10.5.0/3`
- UTC: `UTC0`

Example observer altitude:

- Use meters above sea level, not feet.
- If unknown, use a reasonable local estimate; it mainly affects aircraft distance and overhead filtering.

## Tests

Run the host-side unit suite before compiling or flashing:

```bash
tools/run_unit_tests.sh
```

The tests cover pure logic that can run without the board: aircraft and climate formatting, display-view layout, cascade behavior, config parsing, quiet-hours timing, retained state, and JSON parsers. The runner auto-detects ArduinoJson in the usual Arduino library folders. If ArduinoJson is elsewhere:

```bash
ARDUINO_JSON_INC=/path/to/ArduinoJson/src tools/run_unit_tests.sh
```

## Compile

From this directory:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB" \
  .
```

If your Seeed display library is installed outside Arduino's standard library search path, pass it explicitly:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB" \
  --libraries /path/to/Arduino/libraries \
  .
```

## Flash

Connect the device over USB and find the serial port:

```bash
arduino-cli board list
```

Use the USB Serial Port, typically `/dev/cu.usbserial-*`; skip Bluetooth ports. If no USB serial port appears, press RESET. If upload still cannot connect, hold BOOT, tap RESET, release BOOT, then retry to force ROM bootloader mode.

Upload with the detected `/dev/cu.*` port:

```bash
arduino-cli upload \
  --fqbn "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB" \
  --port /dev/cu.usbserial-10 \
  .
```

Replace `/dev/cu.usbserial-10` with the port shown on your machine.

To compile and upload in one command:

```bash
arduino-cli compile --upload \
  --fqbn "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB" \
  --port /dev/cu.usbserial-10 \
  .
```

Serial debug output from the sketch is on the hardware UART at 115200 baud, GPIO43 TX / GPIO44 RX. That is separate from the USB upload port and upload speed.

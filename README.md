# Sky Overhead

Sky Overhead is an Arduino sketch for the Seeed reTerminal E1001 / XIAO ESP32S3. It shows the nearest overhead aircraft on the e-paper display, including aircraft type, callsign, tail number, airline, route, altitude, trend, and speed. The right side of the display shows the onboard room temperature and humidity sensor.

The sketch is designed to run like a wall appliance: it sleeps between updates, keeps the e-paper image stable when the visible aircraft data has not changed, handles temporary network failures by leaving the last good screen up, and pauses aircraft checks during quiet hours.

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
- `adsbdb.com` for airline and origin/destination lookup by callsign

## Local Files

- `sky_overhead.ino`: main sketch
- `driver.h`: display board selection, currently `BOARD_SCREEN_COMBO 520`
- `IconFont.h`: generated bitmap icon font used by the display UI
- `tools/generate_icon_font.py`: regenerates `IconFont.h` from Lucide SVG sources
- `assets/icons/lucide/`: cached Lucide source SVGs and license

## Runtime Lifecycle

Every update cycle is a full reboot from deep sleep. Nothing runs in the background while the device sleeps; the e-paper panel passively holds the last image.

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

The sketch stores a small amount of state in `RTC_DATA_ATTR`, which survives deep sleep but not a full power loss. That includes the last rendered signature, last-seen aircraft details, last-seen route, timestamp, and redraw count.

## Display Behavior

- The left column shows the active aircraft or the retained last-seen aircraft.
- The right column shows indoor temperature and humidity.
- During quiet hours, the display switches to a moon-icon sleep screen while aircraft checks are paused.
- The footer shows a prominent local `Last refreshed` time.
- Aircraft type code is the primary aircraft label, for example `A321`.
- Callsign and tail number are shown below the type, for example `FIN7EH (OH-LZH)`, with airline below that line.
- Rotorcraft use a helicopter glyph when ADS-B reports rotorcraft category `A7`; there is no type-code fallback.
- When no current aircraft is found, the display keeps the retained aircraft layout and previous aircraft glyph with a prominent `Last refreshed` time.
- Aircraft altitude, vertical trend, and speed are shown as three fields, for example `FL132 | climbing | 307 kts`, with bold dot markers drawn between them.
- Missing aircraft fields are omitted instead of reserving blank rows; following rows move up.
- A signature skip prevents unnecessary e-paper refreshes when visible aircraft state has not changed.
- Every 20 redraws, the sketch runs a full white refresh before the normal draw to reduce accumulated ghosting.

The display is intentionally not always live. The device wakes, fetches current data, decides whether the visible state has changed enough to justify an e-paper refresh, then goes back to deep sleep. This trades second-by-second accuracy for much lower energy use, less e-paper flashing, and less ghosting. When the screen does refresh, it is drawn from the latest data collected during that wake cycle.

Rendering happens in the display library's RAM buffer first. The slow e-paper transfer only happens during `epaper.update()`.

Reusable display glyphs are generated from Lucide SVGs. The generator downloads
and caches missing source SVGs in `assets/icons/lucide/`. After adding or
changing icons, install `rsvg-convert` and ImageMagick's `magick` command, then
run:

```bash
python3 tools/generate_icon_font.py
```

## Hardware Quirks

- Keep `SPIClass spiSD(HSPI)` local inside SD-card functions. A file-scope `SPIClass` can crash on boot because the constructor runs before FreeRTOS is ready.
- Avoid partial e-paper refresh. `updataPartial()` exists in the UC8179 driver, but it produces heavy ghosting with the built-in waveform LUT. Use full `update()` only.
- Use `460800` upload speed. `921600` can drop this USB-serial adapter during the baud switch.
- The SD card slot is explicitly powered only while reading config, then powered down before Wi-Fi and fetch work.

## Arduino Setup

Install Arduino CLI, then install the ESP32 board package and required libraries.

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

Install Seeed_GFX, which provides the reTerminal E Series e-paper `TFT_eSPI.h` / `EPaper` display stack. It is not the stock Bodmer TFT_eSPI library. Install Seeed_GFX into your Arduino libraries folder, for example:

```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/Seeed-Studio/Seeed_GFX.git
```

The sketch includes `driver.h` with:

```c
#define BOARD_SCREEN_COMBO 520
```

That selects Seeed's E1001 display setup (`Setup520_Seeed_reTerminal_E1001`). If compilation fails with missing `TFT_eSPI.h`, `EPaper`, or `EPAPER_ENABLE`, check that Seeed_GFX is installed and that `driver.h` is present next to `sky_overhead.ino`.

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

The device reads its runtime settings from a plain text file at the root of the microSD card:

```text
/config.txt
```

Use a FAT-formatted microSD card. Create `config.txt` in the card root, not inside a folder. The file must use one `KEY=VALUE` pair per line. Do not quote values, and do not add spaces around `=`. Setting names are case-insensitive; documented option values such as `kts`, `metric`, `f`, `true`, and `on` are also case-insensitive.

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

The sketch has defaults for units, radius, and sleep interval. Quiet hours are disabled unless `NIGHT_MODE` is configured. Wi-Fi and observer location must be correct for the app to work usefully. If the SD card is missing or `config.txt` cannot be read, the device will boot, leave Wi-Fi disconnected, and retry after a short sleep.

Example timezone values:

- Budapest/Berlin/Central Europe: `CET-1CEST,M3.5.0,M10.5.0/3`
- UTC: `UTC0`

Example observer altitude:

- Use meters above sea level, not feet.
- If unknown, use a reasonable local estimate; it mainly affects aircraft distance and overhead filtering.

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

Use the port listed as a USB Serial Port, typically `/dev/cu.usbserial-*`. Skip Bluetooth ports. If no USB serial port appears, press RESET. If upload still cannot connect, hold BOOT, tap RESET, release BOOT, then retry to force ROM bootloader mode.

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

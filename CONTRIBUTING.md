# Contributing

Contributions that improve reliability, hardware support, documentation, or the display experience are welcome. Keep changes focused and explain any hardware assumptions.

## Before You Start

- Search existing issues and pull requests for related work.
- Open a feature request before making a substantial behavior or hardware change.
- Never include Wi-Fi credentials, precise private locations, certificates, or other secrets in issues, logs, or commits.

## Development Setup

### Docker Builds (Recommended)

Install Docker with Buildx (included in Docker Desktop), then run from the repository root:

```bash
docker buildx bake
```

This runs the unit tests, generates the icon font, compiles firmware, packages a `v0.0.0` development release, and verifies its checksums. Outputs are exported to `.build/firmware/` and `.build/release/`. These are the same Docker targets used by GitHub Actions.

To run individual targets:

```bash
docker buildx bake tests
docker buildx bake firmware
RELEASE_VERSION=v0.1.0 docker buildx bake release
```

Both firmware and release targets require passing unit tests. `RELEASE_VERSION` controls package filenames; it does not create a Git tag or publish a release. Use a clean checkout of the corresponding tag when reproducing a published release.

The build platform is fixed to `linux/arm64`, running natively on Apple Silicon Docker Desktop and GitHub's `ubuntu-24.04-arm` runners. On x86 hosts, Docker needs ARM64 emulation support; Docker Desktop includes it. The first build downloads the ESP32 toolchain and requires several GB of Docker disk space. Docker caches completed layers; changing source files reruns tests and compilation.

Reproducibility inputs are recorded in the repository:

- `Dockerfile`: Debian base image digest, dated Debian package snapshot, checksum-verified Arduino CLI 1.5.1, fixed build paths, UTC locale settings, and `SOURCE_DATE_EPOCH=1704067200` (2024-01-01 UTC).
- `sketch.yaml`: ESP32 platform, Arduino libraries, and board options.
- `tools/setup_arduino_dependencies.sh`: Seeed_GFX tag and verified commit.
- `tools/generate_icon_font.py`: Lucide source commit.

The fixed epoch stabilizes compiler timestamps and release ZIP metadata; archive entries are sorted and extra ZIP metadata is omitted. Host Arduino configuration, installed libraries, generated fonts, and build outputs are excluded from the Docker context. Dependency downloads still require internet access on uncached builds.

For a fresh compilation without reusing the firmware layer:

```bash
docker buildx bake firmware --set firmware.no-cache-filter=firmware-build
```

Use `--no-cache` to rebuild all layers, including dependency installation. When updating the toolchain, update its pins deliberately and compare the resulting firmware and release checksums across fresh builds.

### Native Development Tools

Install Arduino CLI 1.3.0 or newer, then install the pinned development dependencies:

```bash
tools/setup_arduino_dependencies.sh
```

Run the host-side tests:

```bash
tools/run_unit_tests.sh
```

Compile the firmware with the pinned profile in `sketch.yaml`:

```bash
tools/build_firmware.sh
```

The build wrapper automatically downloads icons from the pinned Lucide commit and regenerates the ignored `IconFont.h` when needed. Install `rsvg-convert` and ImageMagick before the first build; source icons and generated output are not committed.

On macOS:

```bash
brew install librsvg imagemagick
```

On Debian or Ubuntu:

```bash
sudo apt-get install librsvg2-bin imagemagick
```

The setup script installs ArduinoJson and QRCode and checks out the Seeed_GFX `V3.1.0` tag. The build profile in `sketch.yaml` pins the ESP32 platform and indexed libraries, records the board options, and references the separately checked-out Seeed_GFX directory. Seeed_GFX is handled by the setup script because it is not published in the Arduino Library Index.

Use the XIAO ESP32S3 target configured as:

```text
esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB
```

The `460800` upload speed avoids connection loss seen with `921600` on this device's USB-serial adapter.

If `TFT_eSPI.h`, `EPaper`, or `EPAPER_ENABLE` is missing during compilation, verify the Seeed_GFX checkout and `driver.h`. This project uses Seeed's e-paper stack, not the stock Bodmer TFT_eSPI library.

## Tests

The host suite covers logic that can run without the board, including formatting, display layout, configuration parsing, quiet hours, retained state, and JSON parsing. The runner auto-detects ArduinoJson in standard Arduino library directories. To use another location:

```bash
ARDUINO_JSON_INC=/path/to/ArduinoJson/src tools/run_unit_tests.sh
```

## Upload from Source

Connect the device over USB and locate its USB serial port:

```bash
arduino-cli board list
```

Upload an existing build with:

```bash
arduino-cli upload \
  --fqbn "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB" \
  --port <PORT> \
  .
```

To build and upload together:

```bash
tools/build_firmware.sh --upload \
  --fqbn "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,UploadSpeed=460800,FlashSize=8M,PartitionScheme=default_8MB" \
  --port <PORT>
```

If no serial port appears, press RESET. If the upload still cannot connect, hold BOOT, tap RESET, release BOOT, and retry. Firmware debug output uses the separate hardware UART at 115200 baud on GPIO43 TX and GPIO44 RX.

## Debug Configuration

These optional `/config.txt` settings are intended for development rather than normal use:

- `DEMO=1` skips network requests and alternates between sample aircraft and quiet-hours screens for layout work.
- `SD_LOG=1` appends one JSON record to `/screen.log` after each completed content redraw. Each record contains the timestamp, redraw number, screen mode, battery level, and aircraft, climate, and footer text sent to the display.

`SD_LOG` also accepts `true` or `on`. Logging powers and mounts the SD card for each append after the normal configuration read. The file grows until removed or truncated, so enable logging only while debugging.

## Hardware Notes

- Keep `SPIClass spiSD(FSPI)` local to SD-card functions. Seeed_GFX owns `HSPI` for the display, and another controller object for that host can disrupt logging. Constructing `SPIClass` at file scope can also run before FreeRTOS is ready.
- Use full e-paper updates. The UC8179 driver's partial refresh produces heavy ghosting with the built-in waveform table.
- The included `driver.h` selects the E1001 display with `BOARD_SCREEN_COMBO 520`.

Seeed's reTerminal E Series Arduino guides provide additional display and peripheral details:

- https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino/
- https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/

## Making Changes

- Create a focused branch from the latest `main`.
- Follow the existing C++ and Arduino style; avoid unrelated formatting changes.
- Add or update host tests for logic that does not require hardware.
- Keep runtime configuration backward compatible when users may already have deployed SD cards.
- Update `config.example.txt` and the README when configuration behavior changes.
- Test on the reTerminal E1001 when changing display, sleep, SD card, sensor, battery, or networking behavior.

## Pull Requests

Pull requests must pass both required CI checks:

- `Host unit tests`
- `Firmware build`

Describe what changed, why it changed, and how it was tested. Include display photos or screenshots for visible changes when possible. State clearly when hardware validation was not performed.

The repository uses squash merging. Write a concise pull request title suitable for the resulting commit.

# Contributing

Contributions that improve reliability, hardware support, documentation, or the display experience are welcome. Keep changes focused and explain any hardware assumptions.

## Before You Start

- Search existing issues and pull requests for related work.
- Open a feature request before making a substantial behavior or hardware change.
- Never include Wi-Fi credentials, precise private locations, certificates, or other secrets in issues, logs, or commits.

## Development Setup

### Docker Builds (Recommended)

Use Docker for development builds and tests: it provides the pinned toolchain used by CI without installing Arduino or icon-generation tools on your host. Install Docker with Buildx (included in Docker Desktop), then run from the repository root:

```bash
docker buildx bake
```

This runs the unit tests, generates the icon font, compiles firmware, packages a `v0.0.0` development release, and verifies its checksums. Outputs are exported to `.build/firmware/` and `.build/release/`. These are the same Docker targets used by GitHub Actions.

To run individual targets:

```bash
docker buildx bake firmware
RELEASE_VERSION=v0.1.0 docker buildx bake release
```

Both firmware and release targets require passing unit tests. `RELEASE_VERSION` controls package filenames; it does not create a Git tag or publish a release. Use a clean checkout of the corresponding tag when reproducing a published release.

The build platform is fixed to `linux/arm64`, running natively on Apple Silicon Docker Desktop and GitHub's `ubuntu-24.04-arm` runners. On x86 hosts, Docker needs ARM64 emulation support; Docker Desktop includes it. The first build downloads the ESP32 toolchain and requires several GB of Docker disk space. Docker caches dependencies and icon generation before copying source files, so source changes rerun tests and compilation without downloading dependencies again. Release documentation changes rerun only packaging.

Reproducibility inputs are recorded in the repository:

- [`Dockerfile`](Dockerfile): base image, package snapshot, checksum-verified Arduino CLI, fixed build paths, locale, and timestamp epoch.
- [`sketch.yaml`](sketch.yaml): ESP32 platform, Arduino libraries, and board options.
- [`tools/setup_arduino_dependencies.sh`](tools/setup_arduino_dependencies.sh): Seeed_GFX source archive at a pinned commit.
- [`tools/generate_icon_font.py`](tools/generate_icon_font.py): Lucide source commit.

The fixed epoch stabilizes compiler timestamps and release ZIP metadata; archive entries are sorted and extra ZIP metadata is omitted. Host Arduino configuration, installed libraries, generated fonts, and build outputs are excluded from the Docker context. Dependency downloads still require internet access on uncached builds.

For a fresh compilation without reusing the firmware layer:

```bash
docker buildx bake firmware --set firmware.no-cache-filter=firmware-build
```

Use `--no-cache` to rebuild all layers, including dependency installation. When updating the toolchain, update its pins deliberately and compare the resulting firmware and release checksums across fresh builds.

### Native Builds (Alternative)

Install these prerequisites before running the native setup, tests, or build:

- Arduino CLI 1.3.0 or newer
- curl and Python 3.9 or newer
- A C++20-capable host compiler available as `c++`
- `rsvg-convert` and ImageMagick for icon generation

Install the icon tools using your package manager:

On macOS:

```bash
brew install librsvg imagemagick
```

On Debian or Ubuntu:

```bash
sudo apt-get install librsvg2-bin imagemagick
```

From the repository root, install the pinned development dependencies:

```bash
tools/setup_arduino_dependencies.sh
```

Run the [host tests](#tests), then compile with the default profile in `sketch.yaml`:

```bash
tools/build_firmware.sh
```

The setup script installs host-test libraries and QRCode, and downloads Seeed_GFX at the pinned commit because it is not in the Arduino Library Index. The build profile resolves the remaining pinned dependencies. The build wrapper downloads icons from the pinned Lucide commit and regenerates the ignored `IconFont.h` when needed.

The profile's `460800` upload speed avoids connection loss seen with `921600` on this device's USB-serial adapter.

If `TFT_eSPI.h`, `EPaper`, or `EPAPER_ENABLE` is missing during compilation, verify the Seeed_GFX checkout and `driver.h`. This project uses Seeed's e-paper stack, not the stock Bodmer TFT_eSPI library.

## Tests

The host suite covers logic that can run without the board, including formatting, display layout, configuration parsing, quiet hours, retained state, and JSON parsing. Run it with Docker:

```bash
docker buildx bake tests
```

The default Docker build also runs these tests before compiling firmware. For native testing:

```bash
tools/run_unit_tests.sh
```

The runner auto-detects ArduinoJson in standard Arduino library directories. To use another location:

```bash
ARDUINO_JSON_INC=/path/to/ArduinoJson/src tools/run_unit_tests.sh
```

## Upload from Source

### Docker-Built Firmware (Recommended)

Run `docker buildx bake firmware`, then use [FLASHING.md](FLASHING.md#install-esptool) to install esptool on your host and find the device's serial port. Flash the exported merged image:

```bash
esptool --chip esp32s3 --port <PORT> --baud 460800 write-flash 0x0 .build/firmware/sky_overhead.ino.merged.bin
```

### Native Arduino CLI Upload (Alternative)

Connect the device over USB and locate its USB serial port:

```bash
arduino-cli board list
```

Upload an existing build with:

```bash
arduino-cli upload --profile reterminal_e1001 --port <PORT> .
```

To build and upload together:

```bash
tools/build_firmware.sh --upload --port <PORT>
```

If no serial port appears, press RESET. If the upload still cannot connect, hold BOOT, tap RESET, release BOOT, and retry. Firmware debug output uses the separate hardware UART at 115200 baud on GPIO43 TX and GPIO44 RX.

## Debug Configuration

These optional `/config.txt` settings are intended for development rather than normal use:

- `DEMO=1` skips network requests and alternates between sample aircraft and quiet-hours screens for layout work.
- `SD_LOG=1` appends to `/screen.jsonl` in [JSON Lines](https://jsonlines.org/) (JSONL) format, with one JSON object per line after each completed content redraw. Each record contains the timestamp, redraw number, screen mode, battery level, and aircraft, climate, and footer text sent to the display.

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

Pull requests must pass the required `Firmware build` CI check, which runs host unit tests before compiling firmware and validating release packaging. Branch protection requires only `Firmware build`; there is no separate unit-test check.

Describe what changed, why it changed, and how it was tested. Include display photos or screenshots for visible changes when possible. State clearly when hardware validation was not performed.

The repository uses squash merging. Write a concise pull request title suitable for the resulting commit.

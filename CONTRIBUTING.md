# Contributing

Contributions that improve reliability, hardware support, documentation, or the display experience are welcome. Keep changes focused and explain any hardware assumptions.

## Before You Start

- Search existing issues and pull requests for related work.
- Open a feature request before making a substantial behavior or hardware change.
- Never include Wi-Fi credentials, precise private locations, certificates, or other secrets in issues, logs, or commits.

## Development Setup

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
arduino-cli compile .
```

If display glyphs change, install `rsvg-convert` and ImageMagick, regenerate `IconFont.h`, and include both the source icon and generated output:

```bash
python3 tools/generate_icon_font.py
```

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

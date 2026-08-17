# Flashing Prebuilt Firmware

Prebuilt releases target the Seeed reTerminal E1001 with its XIAO ESP32S3. You do not need Arduino CLI or the source code to install the merged firmware image.

## Download

From the [latest GitHub Release](https://github.com/kovaacs/sky_overhead/releases/latest), download:

- `sky-overhead-<VERSION>-merged.bin`
- `SHA256SUMS`

The firmware ZIP contains the separate Arduino build outputs, this guide, and `config.example.txt`. Most users only need the merged binary.

## Verify the Download

On Linux, verify the downloaded merged binary against its entry:

```bash
grep 'merged\.bin$' SHA256SUMS | sha256sum -c -
```

On macOS:

```bash
grep 'merged\.bin$' SHA256SUMS | shasum -a 256 -c -
```

On Windows PowerShell, compare the result with the corresponding line in `SHA256SUMS`:

```powershell
Get-FileHash .\sky-overhead-<VERSION>-merged.bin -Algorithm SHA256
```

## Install esptool

Install [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/) with `pipx`:

```bash
pipx install esptool
```

Alternatively, install it in a dedicated Python virtual environment. This also works with externally managed Python installations, such as Homebrew Python:

```bash
python3 -m venv esptool-venv
source esptool-venv/bin/activate
python -m pip install esptool
```

On Windows PowerShell, activate the environment with `esptool-venv\Scripts\Activate.ps1` instead of the `source` command. Keep the environment active while running the flashing command.

## Find the Serial Port

Connect the reTerminal E1001 over USB. Typical ports are:

- macOS: `/dev/cu.usbserial-*`
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Windows: `COM3`, `COM4`, or another numbered COM port

On macOS, list matching USB serial ports with:

```bash
ls /dev/cu.usbserial-*
```

If no port appears, press RESET once and run the command again.

## Flash

Replace `<PORT>` and `<VERSION>` with your serial port and downloaded release version:

```bash
esptool --chip esp32s3 \
  --port <PORT> \
  --baud 460800 \
  write-flash 0x0 sky-overhead-<VERSION>-merged.bin
```

The merged image contains the bootloader, partition table, and application. Do not run a separate `erase-flash` command: `write-flash` erases the flash sectors covered by the 8 MB merged image automatically.

If esptool cannot connect, hold BOOT, tap RESET, release BOOT, and run the command again. Press RESET once flashing completes if the device does not restart automatically.

## Configure the Device

Copy `config.example.txt` to `config.txt` at the root of a FAT-formatted microSD card. Replace the placeholder Wi-Fi, location, altitude, and timezone values, then insert the card before starting the device.

See the [SD Card Config](README.md#sd-card-config) documentation for every setting and privacy considerations.

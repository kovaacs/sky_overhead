# Flashing Prebuilt Firmware

Prebuilt releases target the Seeed reTerminal E1001 with its XIAO ESP32S3. You do not need Arduino CLI or the source code to install the merged firmware image.

## Download

From the [latest GitHub Release](https://github.com/kovaacs/sky_overhead/releases/latest), download:

- `sky-overhead-<VERSION>-merged.bin`
- `SHA256SUMS`

The firmware ZIP contains the separate Arduino build outputs, this guide, and `config.example.txt`. Most users only need the merged binary.

## Verify the Download

On Linux, after downloading all release assets:

```bash
sha256sum -c SHA256SUMS
```

On macOS:

```bash
shasum -a 256 -c SHA256SUMS
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

Alternatively, install it into your active Python environment:

```bash
python3 -m pip install esptool
```

## Find the Serial Port

Connect the reTerminal E1001 over USB. Typical ports are:

- macOS: `/dev/cu.usbserial-*`
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Windows: `COM3`, `COM4`, or another numbered COM port

## Flash

Replace `<PORT>` and `<VERSION>` with your serial port and downloaded release version:

```bash
esptool --chip esp32s3 \
  --port <PORT> \
  --baud 460800 \
  write-flash 0x0 sky-overhead-<VERSION>-merged.bin
```

The merged image contains the bootloader, partition table, and application. A full flash erase is not required for normal installation or upgrades.

If esptool cannot connect, hold BOOT, tap RESET, release BOOT, and run the command again. Press RESET once flashing completes if the device does not restart automatically.

## Configure the Device

Copy `config.example.txt` to `config.txt` at the root of a FAT-formatted microSD card. Replace the placeholder Wi-Fi, location, altitude, and timezone values, then insert the card before starting the device.

See the [SD Card Config](README.md#sd-card-config) documentation for every setting and privacy considerations.

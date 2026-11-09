# CYD MFA Authenticator Offline

TOTP, HOTP, and Steam Guard authenticator for the **ESP32-2432S028 (CYD)**.
It works without Wi-Fi or a microSD card: configuration and secrets remain in
encrypted internal flash storage.

## Features

- AES-256-GCM vault protected by a PIN;
- touchscreen service entry and management;
- groups, favorites, SHA1/SHA256/SHA512, and 6- or 8-digit codes;
- encrypted backups, time synchronization, and updates over Web Serial;
- offline USB manager contained in a single HTML file;
- complete ready-to-flash firmware.

## Quick start

1. Download or clone the repository and open a terminal in its directory.
2. Connect the CYD with a USB data cable. Disconnect other Espressif boards
   during the first flash so the correct device is detected automatically.
3. Install Python 3, then install esptool:

Linux or macOS:

```sh
python3 -m pip install --upgrade esptool
```

Windows PowerShell or Command Prompt:

```powershell
py -m pip install --upgrade esptool
```

If the `py` launcher is unavailable on Windows, use `python` in its place.

4. Flash the complete firmware. The port is intentionally omitted so esptool
   can discover the connected ESP32 automatically.

Linux or macOS:

```sh
python3 -m esptool --chip esp32 write-flash 0x0 firmware-offline-merged.bin
```

Windows PowerShell or Command Prompt:

```powershell
py -m esptool --chip esp32 write-flash 0x0 firmware-offline-merged.bin
```

If automatic detection finds multiple devices, list the available ports with
`python3 -m serial.tools.list_ports` on Linux/macOS or
`py -m serial.tools.list_ports` on Windows. Run the flash command again with
`--port PORT`, replacing `PORT` with the reported value. Typical values such
as `/dev/ttyUSB0`, `/dev/cu.usbserial-*`, and `COM3` are examples only and must
not be copied without checking your computer.

`write-flash` is the current esptool command. If an older esptool 4.x
installation reports it as invalid and lists `write_flash`, either upgrade
esptool or use `write_flash` for that command.

After flashing, press `RST`. On first boot, calibrate the touchscreen, create
a PIN, and set the UTC time.

If no device is detected, close any serial monitor, check that the cable
supports data, install the USB-to-serial driver required by your board, and
put the board in download mode by holding `BOOT`, pressing and releasing
`RST`, and then releasing `BOOT`.

To synchronize time, manage services, or install later updates, open
[`tools/cyd-manager.html`](tools/cyd-manager.html) in desktop Chrome or Edge.

## Build

[PlatformIO](https://platformio.org/) is required:

```sh
pio run -e prod
```

The application firmware is generated at `.pio/build/prod/firmware.bin`.

## Documentation

See [`OFFLINE.md`](OFFLINE.md) for all features, security details, backups,
and update instructions.

## License

Distributed under the MIT License included in [`LICENSE`](LICENSE).

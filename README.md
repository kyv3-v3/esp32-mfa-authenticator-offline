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

1. Download or clone the repository.
2. Connect the CYD over USB.
3. Install esptool with `python -m pip install esptool`.
4. Flash the complete firmware:

```sh
python -m esptool --chip esp32 --port /dev/ttyUSB0 write_flash 0x0 firmware-offline-merged.bin
```

Replace `/dev/ttyUSB0` with your device port if necessary. On first boot,
calibrate the touchscreen, create a PIN, and set the UTC time.

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

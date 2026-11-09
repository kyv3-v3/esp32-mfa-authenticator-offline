# CYD MFA Authenticator Offline

An offline TOTP, HOTP, and Steam Guard authenticator for the
**ESP32-2432S028 (CYD)**. The firmware runs without Wi-Fi, NTP, or a microSD
card. Authenticator secrets and settings stay in the device's internal flash.

> [!IMPORTANT]
> The board has no battery-backed real-time clock. After a complete power
> loss, set the UTC time on the touchscreen or synchronize it from the local
> USB manager before using time-based codes.

[Features](#features) · [Quick start](#quick-start) ·
[USB manager](#offline-usb-manager) · [Updates](#firmware-files-and-updates) ·
[Build](#build-from-source) · [Security](#security-notes)

## At a glance

| Item | Details |
|---|---|
| Target hardware | ESP32-2432S028, also known as the Cheap Yellow Display |
| Authenticators | TOTP, HOTP, and Steam Guard |
| Algorithms | SHA1, SHA256, and SHA512 |
| Capacity | Up to 100 services in 10 groups |
| Storage | AES-256-GCM encrypted internal flash |
| Connectivity | Offline operation with optional local USB management |
| External storage | No microSD card required |
| Included firmware | Complete first-flash image and application update image |

## Features

### Authenticator support

- TOTP with 6 or 8 digits and periods from 15 to 120 seconds
- HOTP with a persistent counter that advances only when requested
- Steam Guard codes imported from compatible `otpauth://` URIs
- SHA1, SHA256, and SHA512 HMAC algorithms
- Favorites kept at the top of the catalog
- Up to 100 services organized into 10 named groups
- RFC 4226 and RFC 6238 self-tests at startup, including SHA1, SHA256,
  SHA512, and Steam test vectors

### Touchscreen interface

- Guided touchscreen calibration on first boot
- PIN creation and unlock directly on the CYD
- Manual TOTP entry with a service name and Base32 secret
- Secret masking with an optional temporary reveal button
- Service editing for group, algorithm, digit count, period, and order
- Two-step service deletion to prevent accidental removal
- Horizontal swipes to move between groups
- Long press on a code card to view its complete configuration
- Short tap on an HOTP card to advance and save its counter
- Countdown bars for time-based codes
- Manual UTC correction from the `TIME` screen
- Double tap on the main screen to toggle the backlight
- Immediate vault lock and configurable automatic locking
- Screen timeout, brightness, light theme, large text, high contrast, and
  reduced-motion settings
- Short screen transitions, staggered card loading, and progress feedback for
  encryption and save operations
- Factory reset with double confirmation

### Encrypted storage and recovery

- AES-256-GCM encryption with a key derived from the PIN using
  PBKDF2-HMAC-SHA256 and a random salt
- Primary PINs from 6 to 20 digits
- Optional duress PIN that opens a separate decoy vault
- Encrypted backup export and restore
- Emergency packages containing the encrypted vault backup and device settings
- Temporary and recovery files used during saves to reduce the risk of data
  loss after an interrupted write
- Automatic migration of legacy `/totps.json` data after the encrypted vault
  has been created and verified
- Increasing unlock delays from 2 seconds to 5 minutes after failed attempts;
  the failure count survives a restart
- Keys, decoded secrets, and active services cleared from RAM when the vault
  locks

### Offline USB manager

[`tools/cyd-manager.html`](tools/cyd-manager.html) is a single local HTML file
for desktop browsers with Web Serial support, such as Chrome and Edge. It
makes no network requests. The CYD must be unlocked on its touchscreen before
the manager can list or edit services.

The manager can:

- synchronize UTC time from the computer
- import TOTP, HOTP, and Steam `otpauth://` URIs
- search and list the service catalog
- edit names, groups, algorithms, digits, periods, types, counters, and
  favorites
- move services up or down
- create, rename, and delete groups
- change the primary PIN and configure a duress PIN
- download or restore encrypted backups and emergency packages
- change brightness, theme, screen timeout, automatic lock, large text, high
  contrast, and reduced motion
- lock the vault immediately
- show firmware version, build time, uptime, heap, flash, application space,
  SPIFFS use, clock state, and service count
- install application updates over USB with progress reporting

Buttons expose their loading state, status messages are announced to assistive
technology, and animations follow the browser's reduced-motion preference.

## Quick start

### 1. Prepare the computer

Download or clone this repository, then open a terminal in its directory.
Connect the CYD with a USB data cable. Disconnect other Espressif boards during
the first flash so esptool can identify the correct device automatically.

Install Python 3 and esptool.

Linux or macOS:

```sh
python3 -m pip install --upgrade esptool
```

Windows PowerShell or Command Prompt:

```powershell
py -m pip install --upgrade esptool
```

If the `py` launcher is unavailable on Windows, use `python` in its place.

### 2. Flash the complete firmware

The port is omitted intentionally. Esptool enumerates connected serial ports
and looks for an Espressif device.

Linux or macOS:

```sh
python3 -m esptool --chip esp32 write-flash 0x0 firmware-offline-merged.bin
```

Windows PowerShell or Command Prompt:

```powershell
py -m esptool --chip esp32 write-flash 0x0 firmware-offline-merged.bin
```

If automatic detection finds multiple devices, list the ports:

```sh
python3 -m serial.tools.list_ports
```

On Windows, use:

```powershell
py -m serial.tools.list_ports
```

Run the flash command again with `--port PORT`, replacing `PORT` with the value
reported on that computer. `/dev/ttyUSB0`, `/dev/cu.usbserial-*`, and `COM3`
are examples, not fixed settings.

`write-flash` is the current esptool command. If esptool 4.x reports it as
invalid and lists `write_flash`, update esptool or use `write_flash` for that
command.

### 3. Complete first boot

1. Press `RST` after flashing.
2. Complete touchscreen calibration if it appears.
3. Create and confirm a PIN containing 6 to 20 digits.
4. Enter the current UTC date and time, or connect the local USB manager and
   select **Sync UTC time**.

The PIN cannot be recovered. Keep an encrypted backup and store its PIN
separately.

## Daily use

The main toolbar has four actions:

| Action | Purpose |
|---|---|
| `NEW` | Add a TOTP service by entering its name and Base32 secret |
| `MANAGE` | Edit, reorder, or delete saved services |
| `MENU` | Open display, accessibility, group, lock, and reset controls |
| `TIME` | Correct the UTC clock manually |

Swipe horizontally to change the active group. Long press a card for service
details. A short tap on an HOTP card produces the next code and saves the new
counter.

The ESP32 keeps time while it remains powered. A full power loss clears the
software clock because this board has no battery-backed RTC. Synchronizing
from the manager is local: the browser reads the computer clock and sends the
epoch value directly over USB.

## Using the offline USB manager

1. Open [`tools/cyd-manager.html`](tools/cyd-manager.html) in desktop Chrome,
   Edge, or another browser with Web Serial support.
2. Select **Connect CYD** and choose the board's serial port.
3. Unlock the vault on the CYD when you need to view or change protected data.
4. Use **Sync UTC time** after the board has lost power.

The manager communicates only through the selected serial port. Backup files
remain encrypted, but they should still be treated as sensitive data.

## Firmware files and updates

| File | Use | Vault data |
|---|---|---|
| `firmware-offline-merged.bin` | First installation or complete recovery with esptool at address `0x0` | Preserves an existing vault because the image does not contain an empty SPIFFS partition |
| `firmware-offline-update.bin` | Later application updates from the USB manager | Uses the secondary OTA partition and does not overwrite SPIFFS |

For a USB update:

1. Open the manager and connect the unlocked CYD.
2. Choose `firmware-offline-update.bin` under **USB firmware update**.
3. Keep the USB cable connected until the completion message appears.
4. Wait for the CYD to restart, then reconnect it.

The manager rejects the merged image in the update field. Use the merged image
only with esptool.

## Troubleshooting

### No serial device is detected

- Close PlatformIO, Arduino IDE, serial monitors, and other programs using the
  port.
- Confirm that the USB cable supports data, not only charging.
- Install the USB-to-serial driver required by the board.
- On Linux, confirm that your user has permission to access serial devices.
- Enter download mode by holding `BOOT`, pressing and releasing `RST`, and
  then releasing `BOOT`.

### Flashing starts on the wrong board

Disconnect other serial or Espressif devices, or pass the required port with
`--port PORT`.

### The displayed TOTP codes are rejected

Synchronize UTC time again. TOTP verification depends on the device and server
using the same time window.

### The PIN cannot be recovered

On the PIN screen, tap `RESET` twice to erase the vault and start again. Restore
an encrypted backup afterward if one is available.

## Build from source

[PlatformIO](https://platformio.org/) is required.

```sh
pio run -e prod
```

The build writes the application image to `.pio/build/prod/firmware.bin`.
That application image can be installed from the USB manager. The repository's
prebuilt update image is `firmware-offline-update.bin`.

The production configuration targets the ESP32-2432S028 display, touchscreen,
and backlight pins in [`platformio.ini`](platformio.ini). Startup runs the
authenticator vector self-test before the main interface becomes available.

## Repository layout

| Path | Contents |
|---|---|
| [`src/`](src/) | Firmware, touchscreen UI, encrypted vault, MFA code generation, settings, and USB protocol |
| [`tools/cyd-manager.html`](tools/cyd-manager.html) | Standalone offline USB manager |
| [`tests/`](tests/) | Clock input, secret normalization, and authenticator test vectors |
| [`OFFLINE.md`](OFFLINE.md) | Detailed offline operation and recovery notes |
| [`CHECKSUMS.sha256`](CHECKSUMS.sha256) | SHA-256 hashes for the distributed firmware images |

## Security notes

- The project is designed for the ESP32-2432S028. Pin assignments may not
  match other CYD revisions or ESP32 boards.
- USB management trusts the computer connected to the unlocked CYD. Lock the
  vault before connecting it to an untrusted machine.
- A duress PIN opens a separate decoy vault. It does not erase or modify the
  primary vault.
- Factory reset permanently erases both vaults and cannot be undone.
- Losing the PIN and every usable backup means losing access to the stored
  secrets.

## License

Distributed under the MIT License in [`LICENSE`](LICENSE).

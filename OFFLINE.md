# ESP32 MFA Authenticator Offline for ESP32-2432S028

This version does not use Wi-Fi or NTP and does not require a microSD card.
Services, groups, and all TOTP parameters are stored in the CYD's internal
flash memory.

## Flashing

`firmware-offline-merged.bin` contains the bootloader, partition table, and
application. Connect the CYD with a USB data cable, disconnect other Espressif
boards, and run the command for your operating system from the project
directory. The port is intentionally omitted so esptool can discover the
connected ESP32 automatically.

The merged BIN does not include an empty SPIFFS image. Flashing it preserves
an existing internal vault and allows migration from the old format.

Linux or macOS:

```sh
python3 -m pip install --upgrade esptool
python3 -m esptool --chip esp32 write-flash 0x0 firmware-offline-merged.bin
```

Windows PowerShell or Command Prompt:

```powershell
py -m pip install --upgrade esptool
py -m esptool --chip esp32 write-flash 0x0 firmware-offline-merged.bin
```

If the `py` launcher is unavailable on Windows, use `python` in its place.

If automatic detection finds multiple devices, list the available ports with
`python3 -m serial.tools.list_ports` on Linux/macOS or
`py -m serial.tools.list_ports` on Windows. Add `--port PORT` to the flash
command, replacing `PORT` with the value reported on that computer. Names such
as `/dev/ttyUSB0`, `/dev/cu.usbserial-*`, and `COM3` are examples, not fixed
settings.

`write-flash` is the current esptool command. If an older esptool 4.x
installation reports it as invalid and lists `write_flash`, either upgrade
esptool or use `write_flash` for that command.

If no device is detected, close any serial monitor, use a USB data cable, and
install the USB-to-serial driver required by the board. Put the board in
download mode by holding `BOOT`, pressing and releasing `RST`, and then
releasing `BOOT`. Linux users may also need permission to access serial
devices according to their distribution. Press `RST` after a successful
flash.

## First boot

1. Complete touchscreen calibration if requested.
2. Create and confirm a PIN containing 6 to 20 digits. The PIN encrypts the
   internal vault and cannot be recovered.
3. Set the UTC time on the display. Alternatively, open
   `tools/cyd-manager.html` in Chrome or Edge, connect the CYD, and select
   **Sync UTC time**.

The ESP32 keeps time while it remains powered. After a complete power loss,
time must be synchronized again because the board has no battery-backed RTC.
The USB manager performs this operation with one click and without Internet
access: it reads the computer time and sends the epoch value to the CYD.

When the first PIN is created, the firmware automatically migrates any
services found in the old `/totps.json` file in internal flash. The plaintext
copy is removed only after the encrypted vault has been created and verified.
The microSD card is never initialized or required.

## Features on the CYD

- `NEW`: manually enter a name and Base32 secret.
- `MANAGE`: delete services and change their group, SHA1/SHA256/SHA512
  algorithm, 6/8-digit output, 30/60/90-second period, and order.
- `MENU`: view clock status, manage groups, and adjust brightness, theme,
  accessibility profile, screen timeout, automatic lock, immediate lock, and
  factory reset with double confirmation.
- `TIME`: correct the UTC time manually.
- Horizontal swipe: change the active group.
- Long press on a card: show complete service details.
- Short tap on an HOTP service: advance and immediately save its counter.

Screens use short, consistent transitions. TOTP cards appear in sequence,
and encryption or save operations show an activity indicator. Animations are
intentionally lightweight to keep the CYD responsive and limit RAM use.

Manual or automatic locking clears the AES key and decoded secrets from RAM.
TOTP, HOTP, and Steam Guard codes are supported with SHA1/SHA256/SHA512, 6 or
8 digits, and periods from 15 to 120 seconds. Favorites remain at the top of
the catalog.

Failed unlock attempts trigger increasing delays from 2 seconds up to 5
minutes. The attempt count is stored in non-volatile memory, so restarting
the device does not reset this protection.

## Offline USB manager

Open `tools/cyd-manager.html` in desktop Chrome or Edge and select
**Connect CYD**. You can then:

- synchronize time;
- import `otpauth://totp` and `otpauth://hotp` URIs, including Steam Guard;
- search, list, edit, reorder, and favorite services;
- create, rename, and delete groups;
- change the primary PIN and configure a duress PIN;
- export and restore encrypted backups or a complete emergency package;
- view RAM, flash, SPIFFS, uptime, clock, and service-count diagnostics;
- adjust brightness, theme, timeout, large text, high contrast, and reduced
  motion;
- update the firmware over USB without esptool.

The manager is a single local file and makes no network requests. For
security, listing and editing services requires the CYD to be unlocked from
the touchscreen first.

The manager is responsive, displays loading state directly on buttons,
provides accessible success and error feedback, and respects
`prefers-reduced-motion`.

### Updating from the USB manager

The first flash or a complete recovery always requires
`firmware-offline-merged.bin` and the command in the **Flashing** section.
For later updates:

1. open the manager and connect the CYD;
2. select `firmware-offline-update.bin` under **USB firmware update**;
3. do not disconnect the cable before the completion message appears;
4. wait for the restart and reconnect the device.

The manager rejects the merged BIN and displays real progress. The update
uses the second OTA partition and does not overwrite the SPIFFS vault.

## Security and recovery

The vault uses AES-256-GCM with a key derived from the PIN through
PBKDF2-HMAC-SHA256 and a random salt. Each save operation writes to a
temporary file and keeps a recovery copy until the new vault has been
successfully replaced. Exported backups remain encrypted.

The duress PIN opens a separate decoy vault. It displays no warning and does
not delete the real vault. Configure it only while the primary vault is open,
and use a different PIN. To make the decoy believable, unlock it once and add
a few non-sensitive services.

The emergency package contains the still-encrypted backup and device
settings. Treat it as sensitive backup data and store it separately from the
PIN.

There is no way to decrypt the vault if you forget the PIN. On the PIN screen,
tap `RESET` twice to erase it and start over. Keep at least one encrypted
backup and remember its PIN.

## Building

```sh
pio run -e prod
```

At startup, the firmware runs an RFC 4226/6238 self-test for HOTP, TOTP with
SHA1/SHA256/SHA512, and a Steam vector. The compiled application is written
to `.pio/build/prod/firmware.bin`; the copy used by the manager is
`firmware-offline-update.bin`.

# Bitty USB library

The public installer lives in this repository's GitHub Pages deployment. Connect an original M5StickC Plus with a data-capable USB cable and use a desktop browser that exposes Web Serial. The installer checks for that browser capability. ESP Web Tools currently documents desktop Firefox, Chrome and Edge support; iOS is unsupported. Driver requirements depend on the USB bridge and operating system. No Wi-Fi credentials or desktop app are required.

Choose the exact board before flashing. This build is for the original ESP32 M5StickC Plus with 4 MiB flash and no PSRAM. StickC, Plus2, Core and S3 devices need separately tested builds.

## Install and recover

First installation replaces the current firmware. Use the erase option for a fresh device or a migration from the old embedded-cartridge layout. Erasing removes settings, controller bonds, records and cartridges. Back up anything you need first. Close other serial applications and the USB library connection before opening the flasher.

For an existing Bitty USB-library installation with the same partition layout, leave erase disabled to preserve NVS and cartridge storage. The published manifest writes separate preconfigured DIO/80 MHz/4 MiB image parts. It does not write padding across the NVS or library region. Never distribute Arduino's full 4 MiB merged image as a preserving update.

If a CH552 USB bridge stops responding, close the browser connection and other serial handles, then reconnect the cable. Repeated resets while a process holds the port can leave the device in download mode. Preserve the original flash backup when migrating an existing private build. A different partition layout needs an explicit migration.

## Add and remove cartridges

Connect through the library section after Bitty starts. Connecting ends the current game run and pauses gameplay during flash writes. Choose an uncompressed NTSC `.nes` file and a display name. The browser checks its header, supported mapper, region and length. The device checks the complete payload checksum and validates its cartridge adapter before committing it.

Generic NES cartridges use mappers 0, 1, 4, 37 or 118, subject to the stricter bank/header limits in `nes_runtime.h`. The file limit is 1 MiB. Specialized Tetris GYM, Tetris Zero and Geometry Dash adapters require the supported prepared images described in their project docs. No cartridge binaries are included in public firmware.

The device stores up to twelve cartridge records. Multiple generic NES cartridges appear separately in the game picker. Each specialized adapter currently accepts one payload. The combined Tetris menu chooses its single-player or two-player engine. Removing a cartridge frees its allocated flash pages for reuse. Deletion does not securely wipe its bytes.

The resident engines and authored native games are compiled firmware. Removing their menu entries cannot recover code space. Adding native code or changing the resident game set requires a firmware update. This version does not claim to load arbitrary native plug-ins.

## Storage and interruption behavior

The 4 MiB layout retains NVS at `0x9000`, allocates a 2 MiB app at `0x10000`, and allocates a 1,966,080-byte mapped library at `0x210000`. Two 4 KiB directory sectors leave 1,957,888 bytes of cartridge capacity. Payload allocations round up to 4 KiB pages. The manager reports used space and the largest free contiguous block.

Uploads use free pages while the old record stays allocated. A replacement therefore needs enough additional contiguous space for its new payload. If there is insufficient room, the device rejects it before erasing anything. After whole-payload verification, a CRC-protected directory is written to the alternate sector and a commit marker is written last. An interrupted upload leaves the previous committed library available. Corrupt payloads are quarantined from launch and remain listed for deliberate removal/reinstallation.

Directory updates and payload writes are exercised with simulated power loss at every mutation byte. Browser transport tests cover fragmented replies, delayed duplicate responses, checksums and receive bounds. Physical device and browser verification must be recorded separately in each release receipt.

## Build and publish

Run `python tools/setup.py`, then `python tools/build_site.py`. The site packager invokes `tools/build.py --public --library --display stickc-plus`. That path creates project-authored starter art, disables discovery of private ROMs/APKs, verifies image and input hashes, checks the actual compiled partition table, and refuses an app above 2 MiB.

The site is written to ignored `.local/site`. Its installer SDK is built from the locked browser dependencies. `public-origin.json`, the firmware checksum list and manifest accompany the release. Keep private builds separate. The source allowlist and Git history audit run before publication.

The public website at bitty.games is a separate project. It can link to this GitHub installer. No custom domain is required for the installer, and this repository must not take over unrelated Cloudflare DNS records.

## Protocol

Requests are bounded tab-separated lines: `TP1`, a nonzero decimal request ID, command and fields, then a newline. Replies begin with `TP1 ` followed by the echoed request ID and tab-separated response fields. `HELLO`, `LIST`, `BEGIN`, `CHUNK`, `COMMIT`, `ABORT`, `REMOVE`, `BOOT`, `LAUNCH`, `PING` and `EXIT` are supported. Chunks carry at most 256 payload bytes as hex, with an offset and CRC32. The directory remains private to the device. Runtime file data travels over USB only.

Keepalive requests prevent the 30-second idle management timeout. Holding right exits management. Unfinished uploads are aborted on exit. After an uncertain commit acknowledgement, reconnect and inspect the library before retrying. A timeout cannot prove whether the last commit happened.

## Build-output cleanup

Generated firmware, staging, browser dependencies, SDK bundles and test binaries under `.local` are reproducible caches. Remove them after retaining the release receipts and required recovery images. Recreate them with the setup, test and build commands above. Private reference extraction can be rebuilt from the owner's original inputs using the import tools.

## Computer clock setup

After installing, close the installer dialog and choose **Use this computer’s time**. Select the Stick’s USB port. The browser sends its current local year, month, day, hour, minute and second. Connecting the cartridge library also synchronizes the clock. A clock-only connection works with embedded-game builds, keeps the current game running and needs no cartridge partition or Wi-Fi.

The device validates the complete date before changing the clock and replies with the accepted values. **Set clock** and **Set date** remain available on the device. The time uses the computer’s local zone at synchronization. Resync after travel or daylight saving changes. The internal ESP32 RTC timer keeps a synchronized clock through sleep and supported soft resets. Boards without a backed clock need another sync after full power loss. The firmware does not infer a time zone from USB location.

The original CH552 bridge uses the verified 9600-to-115200 opening sequence with DTR and RTS low. StickS3 native USB opens once at 115200 with DTR high and RTS low. Applying the bridge’s control-line sequence to the S3 can reset it repeatedly.

# Development board testing

This branch uses the preview API at `https://board-preview.linaku94.workers.dev/api/display/`
and the separate `dev/manifest-dev.txt` OTA feed, configured in `firmware_config.h`.
Flash `build/Inkplate_Boards.esp32.Inkplate6V2/inkplate_showcase.ino.bin` onto the test
board once (or upload the sketch with Arduino IDE). Subsequent dev releases use OTA.
The firmware checks for OTA every 15 minutes.

Legacy production firmware also polls `dev/manifest.txt`, so that file stays at
the published production version and URL. Do not point it at the development binary.
The release script is restricted to `dev` and updates only `manifest-dev.txt`.
Before promoting to master, change both URLs in `firmware_config.h` to the production
API and master OTA feed, restore the production release workflow, and rebuild.

# Inkplate showcase firmware

Release in one command using Arduino IDE's bundled Arduino CLI and the installed
Inkplate board package:

```bash
./release.sh 1.0.22 --publish
```

This bumps the sketch version, compiles with `PartitionScheme=default` (two OTA
slots), checks the partition table, and commits source, fresh application binary,
and matching development manifest together before pushing the dev GitHub OTA feed.
Compilation failure stops before updating the binary or manifest. Set
`ARDUINO_CLI` and `ARDUINO_CLI_CONFIG` if your IDE is installed elsewhere.
Publication makes the release available to existing devices through OTA;
it does not verify that a device installed it.

`wifistuff.cpp` stays ignored. As in previous releases, the compiled binary contains
Wi-Fi credentials and is published to the existing feed.

## Recovery changes in 1.0.22

- Startup/reconnection no longer erase the physical e-paper frame. Content is
  replaced only after a successful render; empty text and off-screen origins are rejected.
- Image HTTP transfers have a 20-second body deadline and a 1 MiB size limit.
  Incomplete downloads never reach the decoder. PNG/JPEG/BMP formats are detected
  from bytes, so query strings/extensions do not affect decoding. PNG/BMP dimensions
  are bounded to the panel; uncompressed, bottom-up BMPs are supported.
- TLS handshake/connect/read timeouts are explicit. Watchdog resets occur between
  operations, during Wi-Fi attempts, and during OTA download progress.
- OTA checks run every 15 minutes with conditional ETag requests. Only strictly
  newer versioned HTTPS manifests are accepted; failed newer updates remain retryable.
- Unchanged-content health reports run at most once per minute, using an explicit
  TLS client. Scan results are freed after connection attempts.

The hourly reboot remains. Runtime confirmation requires the board's health
endpoint or serial logs. The existing TLS policy is retained: configure
`UPDATE_ROOT_CA` to verify certificates; otherwise clients use `setInsecure()`.

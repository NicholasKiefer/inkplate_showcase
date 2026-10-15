# Development board

Production firmware remains on `master` (this repository has no `main` branch).
The local test board runs `dev`, rebased onto `master`, with preview content and
its own `manifest-dev.txt` OTA feed. Never edit the production `manifest.txt` to
release a development build.

The local board was identified by unplug/reconnect on 2026-10-15:
`/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0` (`/dev/ttyUSB0`),
ESP32 MAC `c0:49:ef:30:65:1c`. Recheck identity if hardware changes.

Firmware 1.0.24 accepts the website text scale range 1–72 and image objects with
relative URLs, and reports the development device ID to preview health.
WiFi credentials remain in ignored `wifistuff.cpp`.

Build/upload using Arduino CLI and FQBN
`Inkplate_Boards:esp32:Inkplate6V2:PartitionScheme=default`.
The default partition is required for two OTA slots. Use the system Python
(`PATH=/usr/bin:/bin:$PATH`) with pyserial installed.

`./release.sh VERSION` builds and commits a development binary and its matching
manifest. `./release.sh VERSION --publish` additionally pushes only `dev`.
It refuses other branches and checks that content and OTA target development.
Production publication requires a separate, explicitly authorized promotion
following physical testing; do not merge development endpoint configuration into
production unchanged.

The production `manifest.txt` retained on dev is a legacy feed for old installed
firmware. Leave it byte-for-byte identical to master. New development firmware
reads only `manifest-dev.txt`, whose binary URL must also target `dev`.

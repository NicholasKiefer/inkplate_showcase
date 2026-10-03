#!/usr/bin/env bash
# Build and commit source + binary + manifest together, then optionally publish.
set -euo pipefail
cd "$(dirname "$0")"
version=${1:?Usage: ./release.sh VERSION [--publish]}
shift
publish=false
if [[ ${1:-} == --publish ]]; then publish=true; shift; fi
(($# == 0)) || { echo "Unknown argument" >&2; exit 2; }
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 2
cli=${ARDUINO_CLI:-$(command -v arduino-cli || true)}
if [[ -z $cli ]]; then
  cli=/var/lib/flatpak/app/cc.arduino.IDE2/current/active/files/arduino-ide/resources/app/lib/backend/resources/arduino-cli
fi
[[ -x $cli ]] || { echo "Set ARDUINO_CLI to Arduino IDE's bundled arduino-cli." >&2; exit 1; }
config=${ARDUINO_CLI_CONFIG:-${HOME}/.arduinoIDE/arduino-cli.yaml}
# The board's first menu choice is huge_app, which has NO second OTA slot.
fqbn=Inkplate_Boards:esp32:Inkplate6V2:PartitionScheme=default
build=$(mktemp -d /tmp/inkplate-release.XXXXXX)
trap 'rm -rf "$build"' EXIT
export PATH=/usr/bin:/bin:$PATH
python3 - "$version" <<'PY'
import pathlib, re, sys
p = pathlib.Path('inkplate_showcase.ino')
s, count = re.subn(r'#define FIRMWARE_VERSION "[0-9.]+"',
                   '#define FIRMWARE_VERSION "' + sys.argv[1] + '"', p.read_text())
assert count == 1
p.write_text(s)
PY
"$cli" compile --config-file "$config" --fqbn "$fqbn" --build-path "$build" "$PWD"
python3 - "$build/inkplate_showcase.ino.partitions.bin" <<'PY'
import pathlib, struct, sys
p = pathlib.Path(sys.argv[1]).read_bytes()
entries = [struct.unpack_from('<HBBII16sI', p, i) for i in range(0, len(p)-31, 32)]
assert any(e[0] == 0x50aa and e[1] == 0 and e[2] == 0x11 for e in entries), 'Missing second OTA slot'
PY
out=build/Inkplate_Boards.esp32.Inkplate6V2
mkdir -p "$out"
cp "$build"/inkplate_showcase.ino*.bin "$out/"
python3 - "$version" <<'PY'
import pathlib, sys
p = pathlib.Path('manifest.txt')
url = p.read_text().splitlines()[1]
assert url.startswith('https://')
p.write_text(sys.argv[1] + '\n' + url + '\n')
PY
git diff --check
git add inkplate_showcase.ino drive_main.cpp drive_main.h wifistuff.h .gitignore release.sh README.md manifest.txt "$out/inkplate_showcase.ino.bin"
git commit -m "Release $version: preserve display and bound network recovery"
if $publish; then git push origin HEAD:master; fi

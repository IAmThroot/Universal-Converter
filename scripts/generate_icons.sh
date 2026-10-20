#!/usr/bin/env bash
# Regenerate the Universal Converter icon set from the SVG masters.
#
#   resources/app_icon.svg       -> master, used for 64px and above
#   resources/app_icon_small.svg  -> optical correction, used for 16/22/24/32px
#
# Requires: rsvg-convert (or inkscape) + ImageMagick.
set -euo pipefail

RES="$(cd "$(dirname "$0")/.." && pwd)/resources"
OUT="$RES/icons"
MASTER="$RES/app_icon.svg"
SMALL="$RES/app_icon_small.svg"

command -v rsvg-convert >/dev/null || { echo "rsvg-convert not found" >&2; exit 1; }
command -v magick     >/dev/null || { echo "magick not found" >&2; exit 1; }

mkdir -p "$OUT"

render() { # render <svg> <size> <dest>
    rsvg-convert -w "$2" -h "$2" "$1" -o "$3"
}

# Large sizes use the master, small sizes use the optical-correction variant.
for s in 512 256 128 64; do
    render "$MASTER" "$s" "$OUT/app_icon_${s}.png"
done
for s in 48 32 24 22 16; do
    render "$MASTER" "$s" "$OUT/app_icon_${s}.png"
done
for s in 32 24 22 16; do
    render "$SMALL" "$s" "$OUT/app_icon_${s}.png"
done

# Legacy single-size entry point (256px) referenced by the .qrc, CMake install
# rules and the NSIS packaging.
cp "$OUT/app_icon_256.png" "$RES/app_icon.png"

# Windows .ico with multiple embedded sizes.
magick \
    "$OUT/app_icon_16.png" \
    "$OUT/app_icon_24.png" \
    "$OUT/app_icon_32.png" \
    "$OUT/app_icon_48.png" \
    "$OUT/app_icon_64.png" \
    "$OUT/app_icon_128.png" \
    "$OUT/app_icon_256.png" \
    "$RES/app_icon.ico"

cp "$RES/app_icon.ico" "$RES/windows/app_icon.ico" 2>/dev/null || true

echo "Icon set regenerated in $OUT"

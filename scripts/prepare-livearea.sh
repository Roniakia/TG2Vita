#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Export formats for Sony's installer. Preserve generated source artwork.
command -v magick >/dev/null 2>&1 || { echo "Install ImageMagick to export LiveArea artwork." >&2; exit 1; }
magick "$root/assets/source/vita-tg-icon.png" -resize 128x128! -strip "PNG8:$root/assets/icon0.png"
magick "$root/assets/source/startup.png" -strip "PNG8:$root/assets/startup.png"
python3 "$root/scripts/check-livearea.py" "$root/assets"

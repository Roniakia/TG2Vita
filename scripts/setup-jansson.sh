#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
archive=$(mktemp "${TMPDIR:-/tmp}/vita-tg-jansson.XXXXXX")
trap 'rm -f "$archive"' EXIT HUP INT TERM
curl -fL --connect-timeout 10 --max-time 120 \
  https://github.com/akheron/jansson/releases/download/v2.15.1/jansson-2.15.1.tar.gz -o "$archive"
expected=0c7114dc0b2d22a670724a1f95922029d7077c19dbf79a584cb8084d2f267f2f
actual=$(shasum -a 256 "$archive" | awk '{print $1}')
if [ "$actual" != "$expected" ]; then
  echo "Jansson archive checksum mismatch" >&2
  exit 1
fi
mkdir -p "$root/.deps"
tar -xzf "$archive" -C "$root/.deps"
echo "Pinned Jansson source ready."

#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/vita-tg-media-tests.XXXXXX")
trap 'rm -rf "$work"' EXIT
sdk_root=$(xcrun --show-sdk-path)
png_root=${VITA_TG_HOST_PNG:-/opt/homebrew/opt/libpng}
jpeg_root=${VITA_TG_HOST_JPEG:-/opt/homebrew/opt/jpeg-turbo}
xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra \
  -I"$root/tests/media-host" -I"$png_root/include" -I"$jpeg_root/include" \
  "$root/src/ui/media_image.cpp" "$root/tests/media_image_test.cpp" \
  -L"$png_root/lib" -L"$jpeg_root/lib" -lpng -ljpeg -o "$work/media_image_test"
"$work/media_image_test" "$work"

#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/vita-tg-player-tests.XXXXXX")
trap 'rm -rf "$work"' EXIT
sdk_root=$(xcrun --show-sdk-path)
xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra -pthread \
  -I"$root/tests/player-host" -I"$root/src" "$root/src/ui/media_viewer.cpp" \
  "$root/tests/media_viewer_test.cpp" -o "$work/player_test"
"$work/player_test"

#!/bin/sh
# Optional compatibility setup for the inspected Intel VitaSDK on macOS.
# Supply the official zstd 1.5.7 release source archive; no SDK files are changed.
set -eu
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 /path/to/zstd-1.5.7.tar.gz" >&2
  exit 2
fi
if [ "$(uname -s)" != Darwin ]; then
  echo "This compatibility setup is only needed for the legacy macOS SDK." >&2
  exit 2
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
archive=$1
expected=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
actual=$(shasum -a 256 "$archive" | awk '{print $1}')
if [ "$actual" != "$expected" ]; then
  echo "Archive checksum does not match zstd 1.5.7." >&2
  exit 1
fi
scratch=$(mktemp -d "${TMPDIR:-/tmp}/vita-tg-host.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
tar -xzf "$archive" -C "$scratch"
cmake -S "$scratch/zstd-1.5.7/build/cmake" -B "$scratch/build" \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_BUILD_TYPE=Release \
  -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_STATIC=OFF -DZSTD_BUILD_TESTS=OFF
cmake --build "$scratch/build" --parallel "${BUILD_JOBS:-2}"
mkdir -p "$root/.host-libs"
cp -P "$scratch/build/lib/"libzstd*.dylib "$root/.host-libs/"
echo "Legacy host library ready. Run ./scripts/build.sh."

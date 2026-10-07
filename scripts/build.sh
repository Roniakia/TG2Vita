#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${VITASDK:?Set VITASDK to your installed SDK directory}"
if [ -d "$root/.host-libs" ]; then
  export VITA_HOST_LIBRARY_PATH="${VITA_HOST_LIBRARY_PATH:-$root/.host-libs}"
fi
cmake -S "$root" -B "$root/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" --parallel "${BUILD_JOBS:-2}"

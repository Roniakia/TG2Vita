#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${VITASDK:?Set VITASDK to the installed SDK}"
source_dir="$root/.deps/tdlib"
revision=$(cat "$root/ports/tdlib/REVISION")
if [ ! -d "$source_dir/.git" ]; then
  echo "Fetch TDLib first: git clone https://github.com/tdlib/td.git .deps/tdlib" >&2
  echo "Then: git -C .deps/tdlib checkout $revision" >&2
  exit 1
fi
if [ "$(git -C "$source_dir" rev-parse HEAD)" != "$revision" ]; then
  echo "TDLib HEAD does not match ports/tdlib/REVISION" >&2
  exit 1
fi
if git -C "$source_dir" apply --check "$root/ports/tdlib/vita.patch"; then
  git -C "$source_dir" apply "$root/ports/tdlib/vita.patch"
elif ! git -C "$source_dir" apply --reverse --check "$root/ports/tdlib/vita.patch"; then
  echo "TDLib working tree does not match the supported port patch" >&2
  exit 1
fi
# Native generators must run on the development computer, not the Vita.
host_flags=""
if [ "$(uname -s)" = Darwin ]; then
  host_sdk=$(xcrun --show-sdk-path)
  host_flags="-isystem $host_sdk/usr/include/c++/v1"
fi
cmake -S "$source_dir" -B "$root/build/tdlib-host" \
  -DTD_GENERATE_SOURCE_FILES=ON -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$host_flags"
cmake --build "$root/build/tdlib-host" --target prepare_cross_compiling --parallel "${BUILD_JOBS:-3}"
if [ -d "$root/.host-libs" ]; then
  export VITA_HOST_LIBRARY_PATH="${VITA_HOST_LIBRARY_PATH:-$root/.host-libs}"
fi
export VITA_TG_USE_CLANG=1
cmake -Wno-deprecated -S "$source_dir" -B "$root/build/tdlib-clang" \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/vita-toolchain.cmake" \
  -DTD_VITA_PORT_DIR="$root/ports/tdlib" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_CXX_FLAGS=-fshort-enums -DCMAKE_C_FLAGS=-fshort-enums \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DOPENSSL_INCLUDE_DIR="$VITASDK/arm-vita-eabi/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$VITASDK/arm-vita-eabi/lib/libcrypto.a" \
  -DOPENSSL_SSL_LIBRARY="$VITASDK/arm-vita-eabi/lib/libssl.a"
cmake --build "$root/build/tdlib-clang" --target tdjson_static --parallel "${BUILD_JOBS:-3}"

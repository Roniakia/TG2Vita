#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/vita-tg-updater-tests.XXXXXX")
trap 'rm -rf "$work"' EXIT
cmake -S "$root/.deps/jansson-2.15.1" -B "$work/jansson" -DJANSSON_WITHOUT_TESTS=ON -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF > "$work/config.log" 2>&1
cmake --build "$work/jansson" --parallel 2 > "$work/build.log" 2>&1
mkdir -p "$work/include"
cp -R "${VITASDK:-/usr/local/vitasdk}/arm-vita-eabi/include/curl" "$work/include/curl"
openssl_root=${VITA_TG_HOST_OPENSSL:-/opt/homebrew/opt/openssl@3}
sdk_root=$(xcrun --show-sdk-path)
xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra -Wno-deprecated-declarations -I"$root/tests/update-host" -I"$root/src" -I"$root/build/generated" -I"$work/include" -I"$root/.deps/jansson-2.15.1/src" -I"$work/jansson/include" -I"$openssl_root/include" "$root/tests/updater_test.cpp" "$root/src/update/updater.cpp" "$work/jansson/lib/libjansson.a" -L"$openssl_root/lib" -lcrypto -o "$work/updater_test"
(cd "$work" && ./updater_test)

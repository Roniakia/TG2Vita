#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/vita-tg-data-tests.XXXXXX")
trap 'rm -rf "$work"' EXIT
cmake -S "$root/.deps/jansson-2.15.1" -B "$work/jansson" -DJANSSON_WITHOUT_TESTS=ON -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF > "$work/config.log" 2>&1
cmake --build "$work/jansson" --parallel 2 > "$work/build.log" 2>&1
openssl_root=${VITA_TG_HOST_OPENSSL:-/opt/homebrew/opt/openssl@3}
sdk_root=$(xcrun --show-sdk-path)
xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra -I"$root/tests/host" -I"$root/src" -I"$root/build/generated" -I"$root/.deps/tdlib" -I"$root/build/tdlib-clang" \
  -I"$root/.deps/jansson-2.15.1/src" -I"$work/jansson/include" -I"$openssl_root/include" \
  "$root/tests/data_test.cpp" "$root/src/telegram/auth.cpp" "$work/jansson/lib/libjansson.a" \
  -L"$openssl_root/lib" -lcrypto -o "$work/data_test"
"$work/data_test"

xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra \
  -I"$root/src" "$root/tests/conversation_layout_test.cpp" -o "$work/conversation_layout_test"
"$work/conversation_layout_test"

xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra \
  -I"$root/src" "$root/tests/navigation_test.cpp" -o "$work/navigation_test"
"$work/navigation_test"

xcrun clang++ -isysroot "$sdk_root" -isystem "$sdk_root/usr/include/c++/v1" -std=c++17 -Wall -Wextra -I"$root/src" -I"$root/.deps/jansson-2.15.1/src" -I"$work/jansson/include" "$root/tests/update_release_test.cpp" "$work/jansson/lib/libjansson.a" -o "$work/update_release_test"
"$work/update_release_test"

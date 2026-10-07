#!/bin/sh
set -eu
: "${VITASDK:?Set VITASDK to the installed SDK}"
compile=false
for arg in "$@"; do
  case "$arg" in -c|-E|-S|--version|-v|-print-sysroot) compile=true;; esac
done
if [ "$compile" = false ]; then
  exec "$(dirname "$0")/vita-cxx.sh" "$@"
fi
exec /usr/bin/clang++ --target=armv7a-none-eabi -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb -fshort-enums -femulated-tls \
  -D__vita__=1 --sysroot="$VITASDK/arm-vita-eabi" \
  -isystem "$VITASDK/arm-vita-eabi/include/c++/10.3.0" \
  -isystem "$VITASDK/arm-vita-eabi/include/c++/10.3.0/arm-vita-eabi" \
  -isystem "$VITASDK/arm-vita-eabi/include/c++/10.3.0/backward" "$@"

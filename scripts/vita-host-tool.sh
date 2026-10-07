#!/bin/sh
set -eu
: "${VITASDK:?Set VITASDK to the installed SDK}"
tool=$1
shift
if [ "$(uname -s)" = Darwin ]; then
  export DYLD_FALLBACK_LIBRARY_PATH="/usr/lib${DYLD_FALLBACK_LIBRARY_PATH:+:$DYLD_FALLBACK_LIBRARY_PATH}"
  if [ -n "${VITA_HOST_LIBRARY_PATH:-}" ]; then
    export DYLD_LIBRARY_PATH="$VITA_HOST_LIBRARY_PATH${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
  fi
fi
exec "$VITASDK/bin/$tool" "$@"

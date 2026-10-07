#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -lt 1 ]; then
  echo 'Usage: scripts/install-config-vita.sh /Volumes/VITA_USB [--replace]' >&2
  exit 1
fi
vita_root=$1
shift
exec python3 "$root/scripts/install-config.py" --source "$root/secrets/telegram.conf" --vita-root "$vita_root" "$@"

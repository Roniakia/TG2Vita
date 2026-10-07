#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
target_root="$HOME/Library/Application Support/Vita3K/Vita3K/fs/ux0/data/vita-tg"
exec python3 "$root/scripts/install-config.py" --source "$root/secrets/telegram.conf" --destination "$target_root" "$@"

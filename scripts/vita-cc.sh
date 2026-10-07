#!/bin/sh
set -eu
exec "$(dirname "$0")/vita-host-tool.sh" arm-vita-eabi-gcc "$@"

#!/usr/bin/env python3
"""Provision local app credentials separately from the distributable VPK."""
import argparse
import errno
import os
from pathlib import Path
import re
import sys
import tempfile


def validate(data):
    try:
        lines = data.decode('utf8').splitlines()
    except UnicodeError:
        raise ValueError('Configuration must be UTF-8') from None
    values = {}
    for line in lines:
        if len(line.encode('utf8')) > 512:
            raise ValueError('Configuration line is too long')
        if not line or line.startswith('#'):
            continue
        if '=' not in line:
            raise ValueError('Invalid configuration format')
        key, value = line.split('=', 1)
        if key in values:
            raise ValueError('Duplicate configuration setting')
        if key not in {'api_id', 'api_hash', 'use_test_dc'}:
            raise ValueError('Unknown configuration setting')
        values[key] = value
    app_id = values.get('api_id', '')
    if not re.fullmatch(r'[0-9]+', app_id) or not 0 < int(app_id) <= 2147483647:
        raise ValueError('Fill a valid application api_id in secrets/telegram.conf')
    if not re.fullmatch(r'[a-fA-F0-9]{32}', values.get('api_hash', '')):
        raise ValueError('Fill a valid application api_hash in secrets/telegram.conf')
    if values.get('use_test_dc', 'true') not in {'true', 'false'}:
        raise ValueError('Invalid use_test_dc setting')
    return values


def install(data, destination, replace=False, device=False):
    validate(data)
    target = destination / 'telegram.conf'
    if target.is_symlink():
        raise ValueError('Configuration destination must not be a symlink')
    if target.exists():
        if target.read_bytes() == data:
            print('Configuration is already installed; no changes needed.')
            return
        if not replace:
            raise ValueError('Existing configuration kept. Use --replace to replace it deliberately.')
    destination.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination, prefix='.telegram-conf-', delete=False) as file:
            temporary = Path(file.name)
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        try:
            temporary.chmod(0o600)
        except OSError as error:
            # FAT/exFAT USB exports may not provide per-file Unix permissions.
            if not device or error.errno not in {errno.EPERM, errno.EACCES, errno.ENOTSUP}:
                raise
        os.replace(temporary, target)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print('Configuration installed locally. Relaunch Vita TG to load it.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--vita-root', type=Path)
    mode.add_argument('--destination', type=Path)
    mode.add_argument('--check-only', action='store_true')
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    data = args.source.read_bytes()
    validate(data)
    if args.check_only:
        print('Local configuration is valid. Credential values were not displayed.')
        return
    if args.vita_root:
        root = args.vita_root
        # Do not create paths under a disconnected /Volumes mount by accident.
        if not (root / 'data').is_dir() or not (root / 'app' / 'VTGC00001' / 'eboot.bin').is_file():
            raise ValueError('Select the mounted Vita ux0 volume containing app/VTGC00001/eboot.bin and data/')
        destination = root / 'data' / 'vita-tg'
    else:
        destination = args.destination
    install(data, destination, args.replace, device=bool(args.vita_root))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as error:
        print(f'Configuration provisioning failed: {error}', file=sys.stderr)
        sys.exit(1)

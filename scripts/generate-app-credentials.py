#!/usr/bin/env python3
"""Inject app-owned release credentials into an ignored build header."""
import argparse
import importlib.util
import os
from pathlib import Path
import sys
import tempfile


def generate(source, output, enabled):
    app_id, app_hash = 0, ''
    if enabled:
        spec = importlib.util.spec_from_file_location('config_provisioning', Path(__file__).with_name('install-config.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        values = module.validate(source.read_bytes())
        # A distributed client uses production; test-DC selection belongs to
        # config-only development builds and isolated test harnesses.
        app_id, app_hash = int(values['api_id']), values['api_hash']
    text = ('// Generated locally. Do not commit or print this file.\n#pragma once\n'
            'namespace telegram::build_credentials {\n'
            f'inline constexpr bool enabled = {str(enabled).lower()};\n'
            f'inline constexpr int api_id = {app_id};\n'
            f'inline constexpr const char* api_hash = "{app_hash}";\n'
            '}\n')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', dir=output.parent, delete=False) as file:
            temporary = Path(file.name)
            file.write(text)
        temporary.chmod(0o600)
        os.replace(temporary, output)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--enabled', choices=['ON', 'OFF'], required=True)
    args = parser.parse_args()
    try:
        generate(args.source, args.output, args.enabled == 'ON')
    except (OSError, ValueError):
        # Never include file contents or credentials in a build failure.
        print('Unable to prepare app credentials. Fill secrets/telegram.conf or configure VITA_TG_EMBED_APP_CREDENTIALS=OFF for a config-only build.', file=sys.stderr)
        sys.exit(1)

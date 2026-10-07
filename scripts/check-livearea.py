#!/usr/bin/env python3
"""Check installer-facing artwork before packaging; uses only the standard library."""
import argparse
import struct
import sys
import zlib
from pathlib import Path
from zipfile import ZipFile


def check_png(data, label, dimensions):
    if not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError(f'{label}: invalid PNG signature')
    chunks = []
    offset = 8
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError(f'{label}: truncated PNG chunk')
        size = struct.unpack_from('>I', data, offset)[0]
        end = offset + 12 + size
        if end > len(data):
            raise ValueError(f'{label}: truncated PNG data')
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        checksum = struct.unpack_from('>I', data, offset + 8 + size)[0]
        if zlib.crc32(kind + payload) != checksum:
            raise ValueError(f'{label}: invalid PNG checksum')
        chunks.append((kind, payload))
        offset = end
        if kind == b'IEND':
            break
    if offset != len(data) or not chunks or chunks[0][0] != b'IHDR' or chunks[-1] != (b'IEND', b''):
        raise ValueError(f'{label}: invalid PNG structure')
    header = chunks[0][1]
    if len(header) != 13:
        raise ValueError(f'{label}: invalid PNG header')
    width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', header)
    if (width, height) != dimensions:
        raise ValueError(f'{label}: expected {dimensions[0]} x {dimensions[1]}')
    if (depth, color, compression, filtering, interlace) != (8, 3, 0, 0, 0):
        raise ValueError(f'{label}: must be an 8-bit indexed, non-interlaced PNG; run scripts/prepare-livearea.sh')
    palettes = [value for kind, value in chunks if kind == b'PLTE']
    if len(palettes) != 1 or not 0 < len(palettes[0]) <= 768 or len(palettes[0]) % 3:
        raise ValueError(f'{label}: invalid PNG palette')
    pixels = zlib.decompress(b''.join(value for kind, value in chunks if kind == b'IDAT'))
    if len(pixels) != (width + 1) * height:
        raise ValueError(f'{label}: invalid image data size')
    print(f'{label}: {width}x{height}, indexed PNG, valid')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', nargs='?', type=Path)
    parser.add_argument('--vpk', type=Path)
    args = parser.parse_args()
    if args.vpk:
        with ZipFile(args.vpk) as archive:
            if archive.testzip() is not None:
                raise ValueError('VPK ZIP integrity check failed')
            check_png(archive.read('sce_sys/icon0.png'), 'VPK icon0.png', (128, 128))
            check_png(archive.read('sce_sys/livearea/contents/startup.png'), 'VPK startup.png', (280, 158))
    elif args.assets:
        check_png((args.assets / 'icon0.png').read_bytes(), 'icon0.png', (128, 128))
        check_png((args.assets / 'startup.png').read_bytes(), 'startup.png', (280, 158))
    else:
        parser.error('Supply an assets directory or --vpk')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, zlib.error) as error:
        print(f'LiveArea check failed: {error}', file=sys.stderr)
        sys.exit(1)

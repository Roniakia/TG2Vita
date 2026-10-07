#!/usr/bin/env python3
"""Reject linker-generated fixed-address jumps without rebasing relocations."""
import struct
import sys
from pathlib import Path


def check(path):
    data = Path(path).read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected little-endian ELF32')
    u32 = lambda offset: struct.unpack_from('<I', data, offset)[0]
    phoff, shoff = u32(28), u32(32)
    phsize, phnum, shsize, shnum = struct.unpack_from('<HHHH', data, 42)
    loads = []
    for i in range(phnum):
        kind, off, addr, _, size, _, _, _ = struct.unpack_from('<8I', data, phoff + i * phsize)
        if kind == 1:
            loads.append((addr, off, size))
    sections = [struct.unpack_from('<10I', data, shoff + i * shsize) for i in range(shnum)]
    relocations = set()
    for section in sections:
        if section[1] == 9:  # SHT_REL
            for offset in range(section[4], section[4] + section[5], section[9]):
                relocations.add(u32(offset))
    def read_word(addr):
        for base, off, size in loads:
            if base <= addr and addr + 4 <= base + size:
                return u32(off + addr - base)
        return None
    unsafe = []
    count = 0
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        strings_section = sections[section[6]]
        strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
        for offset in range(section[4], section[4] + section[5], section[9]):
            name_offset, addr, size, info, other, index = struct.unpack_from('<IIIBBH', data, offset)
            name_end = strings.find(b'\0', name_offset)
            name = strings[name_offset:name_end].decode('utf8', 'replace')
            if not name.endswith('_veneer') or info & 15 != 2:
                continue
            count += 1
            addr &= ~1
            literal = None
            if read_word(addr) == 0xe51ff004:  # ldr pc, [pc, #-4]
                literal = addr + 4
            elif read_word(addr) == 0xe7fd4778 and read_word(addr + 4) == 0xe59fc000 and read_word(addr + 8) == 0xe12fff1c:
                literal = addr + 12  # Thumb -> ARM -> absolute target
            if literal is not None and literal not in relocations:
                target = read_word(literal)
                if target is not None and any(base <= (target & ~1) < base + length for base, _, length in loads):
                    unsafe.append(f'{name} at {addr:#x}')
    if unsafe:
        raise ValueError(f'{len(unsafe)} unrelocated absolute veneers; enable --pic-veneer. First: {unsafe[0]}')
    print(f'Link veneer check passed: {count} veneers, no unrelocated fixed-address targets')


if __name__ == '__main__':
    try:
        if len(sys.argv) != 2:
            raise ValueError('Usage: check-link-veneers.py executable.elf')
        check(sys.argv[1])
    except (OSError, ValueError, struct.error) as error:
        print(f'Link veneer check failed: {error}', file=sys.stderr)
        sys.exit(1)

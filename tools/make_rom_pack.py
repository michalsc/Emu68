#!/usr/bin/env python3
"""Build an Emu68 multi-Kickstart ROM pack without redistributing ROMs."""

import argparse
import struct
from pathlib import Path

MAGIC = b"EM68ROM\0"
VALID_SIZES = {256 * 1024, 512 * 1024, 1024 * 1024, 2048 * 1024}
MAX_ROMS = 16
ENTRY_SIZE = 48


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("roms", nargs="+", type=Path)
    args = parser.parse_args()
    if len(args.roms) > MAX_ROMS:
        parser.error(f"a pack can contain at most {MAX_ROMS} ROMs")

    images = []
    for path in args.roms:
        data = path.read_bytes()
        if len(data) not in VALID_SIZES:
            parser.error(f"{path}: unsupported size {len(data)} bytes")
        images.append((path.name.encode("utf-8")[:39], data))

    offset = 16 + len(images) * ENTRY_SIZE
    entries = []
    payload = bytearray()
    for name, data in images:
        entries.append(struct.pack("<II40s", offset, len(data), name))
        payload.extend(data)
        offset += len(data)

    args.output.write_bytes(MAGIC + struct.pack("<II", 1, len(images)) + b"".join(entries) + payload)
    print(f"Wrote {args.output} with {len(images)} ROM slot(s)")


if __name__ == "__main__":
    main()

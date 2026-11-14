"""Validate the exact new PORTHEON engine image before enabling native hooks."""

import hashlib
import struct
import sys
from pathlib import Path


ENGINE_SHA256 = "ace7a59392b47a776d469768f4ed1aef2c3395c26ac4ed57fd67661c2d6abe48"
HOOKS = {
    0xE0A930: bytes.fromhex("55415741565657534881ecc8000000"),
    0xE0AAC0: bytes.fromhex("5541574156415541545657534881ecc8000000"),
    0xE0A5B0: bytes.fromhex("5541565657534883ec20488d6c2420"),
}


def check(path: Path) -> None:
    data = path.read_bytes()
    assert hashlib.sha256(data).hexdigest() == ENGINE_SHA256, "new engine hash differs"
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe : pe + 4] == b"PE\0\0"
    assert struct.unpack_from("<I", data, pe + 8)[0] == 0x6AF7A2F1
    assert struct.unpack_from("<I", data, pe + 24 + 56)[0] == 0x3C1C000
    count, size = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
    sections = []
    for i in range(count):
        off = pe + 24 + size + i * 40
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", data, off + 8)
        sections.append((rva, max(virtual_size, raw_size), raw))
    for rva, expected in HOOKS.items():
        offset = next(raw + rva - base for base, length, raw in sections if base <= rva < base + length)
        assert data[offset : offset + len(expected)] == expected, f"hook changed at RVA {rva:x}"
    print("PASS: exact new engine and all three checked controller prologues")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_new_engine_profile.py <UFC 6 novo.exe>")
    check(Path(sys.argv[1]))

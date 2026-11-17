"""Decode a bounded in-memory UFC 6 service response without changing it."""
from __future__ import annotations

import argparse
import ctypes as ct
import json
import math
import struct


kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = (ct.c_ulong, ct.c_int, ct.c_ulong)
kernel.OpenProcess.restype = ct.c_void_p
kernel.ReadProcessMemory.argtypes = (ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t))
kernel.ReadProcessMemory.restype = ct.c_int
kernel.CloseHandle.argtypes = (ct.c_void_p,)


def read(pid: int, address: int, length: int) -> bytes:
    handle = kernel.OpenProcess(0x1010, 0, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess failed")
    try:
        buffer = ct.create_string_buffer(length)
        count = ct.c_size_t()
        if not kernel.ReadProcessMemory(handle, ct.c_void_p(address), buffer, length, ct.byref(count)) or count.value != length:
            raise OSError(ct.get_last_error(), "ReadProcessMemory failed")
        return buffer.raw
    finally:
        kernel.CloseHandle(handle)


class Parser:
    def __init__(self, data: bytes):
        self.data = data
        self.at = 0
        self.entries = 0

    def byte(self) -> int:
        if self.at >= len(self.data):
            raise ValueError("response ended early")
        result = self.data[self.at]
        self.at += 1
        return result

    def string(self) -> str:
        end = self.data.find(b"\0", self.at, min(len(self.data), self.at + 4097))
        if end < 0:
            raise ValueError(f"unterminated string at +0x{self.at:x}")
        result = self.data[self.at:end].decode("utf-8", "replace")
        self.at = end + 1
        return result

    def value(self, depth: int = 0):
        if depth > 32 or self.entries > 100000:
            raise ValueError("response limits exceeded")
        tag_at = self.at
        tag = self.byte()
        if tag == 0x01:
            result = {}
            while self.byte() != 0x09:
                self.at -= 1
                if self.byte() != 0x08:
                    raise ValueError(f"expected object key at +0x{self.at:x}")
                key = self.string()
                result[key] = self.value(depth + 1)
                self.entries += 1
            return result
        if tag == 0x00:
            result = []
            while self.byte() != 0x09:
                self.at -= 1
                if self.byte() != 0x08:
                    raise ValueError(f"expected array element at +0x{self.at:x}")
                result.append(self.value(depth + 1))
                self.entries += 1
            return result
        if tag == 0x03:
            if self.at + 8 > len(self.data):
                raise ValueError("number ended early")
            number = struct.unpack_from("<d", self.data, self.at)[0]
            self.at += 8
            if not math.isfinite(number):
                raise ValueError("nonfinite number")
            return number
        if tag == 0x04:
            return self.string()
        if tag == 0x02:
            return self.byte() != 0
        raise ValueError(f"unknown value tag 0x{tag:02x} at +0x{tag_at:x}")


def decode(data: bytes) -> tuple[dict, int]:
    parser = Parser(data)
    result = {}
    while parser.byte() != 0x09:
        parser.at -= 1
        if parser.byte() != 0x08:
            raise ValueError(f"expected top-level key at +0x{parser.at:x}")
        key = parser.string()
        result[key] = parser.value()
    return result, parser.at


if __name__ == "__main__":
    arg = argparse.ArgumentParser()
    arg.add_argument("pid", type=int)
    arg.add_argument("address", type=lambda x: int(x, 0))
    arg.add_argument("--length", type=lambda x: int(x, 0), default=0x20000)
    arg.add_argument("--full", action="store_true")
    arg.add_argument("--keys", action="store_true")
    args = arg.parse_args()
    response, consumed = decode(read(args.pid, args.address, args.length))
    print(f"Consumed 0x{consumed:x} bytes")
    if args.full:
        print(json.dumps(response, indent=2))
    else:
        output = response.get("Output", {})
        if args.keys:
            print("Output keys:", sorted(output))
            for corner in ("RED_CORNER_FIGHTER", "BLUE_CORNER_FIGHTER"):
                fighter = output.get(corner, {})
                print(corner, "keys:", sorted(fighter))
                print(corner, "TOTAL keys:", sorted(fighter.get("TOTAL", {})))
        for corner in ("RED_CORNER_FIGHTER", "BLUE_CORNER_FIGHTER"):
            fighter = output.get(corner, {})
            print(corner, "rounds", len(fighter.get("ROUNDS", [])))
            for number, round_stats in enumerate(fighter.get("ROUNDS", []), 1):
                print(number, {key: round_stats.get(key) for key in (
                    "KNOCKDOWNS_LANDED", "SIGNIFICANT_STRIKES_THROWN",
                    "SIGNIFICANT_STRIKES_LANDED", "STRIKES_THROWN",
                    "STRIKES_LANDED", "TAKEDOWNS_ATTEMPTED", "TAKEDOWNS_LANDED",
                    "GRAPPLE_DURATION", "JUDGES_SCORES")})
            print("total", {key: fighter.get("TOTAL", {}).get(key) for key in (
                "KNOCKDOWNS_LANDED", "SIGNIFICANT_STRIKES_THROWN",
                "SIGNIFICANT_STRIKES_LANDED", "STRIKES_THROWN",
                "STRIKES_LANDED", "TAKEDOWNS_ATTEMPTED", "TAKEDOWNS_LANDED",
                "GRAPPLE_DURATION", "JUDGES_SCORES")})

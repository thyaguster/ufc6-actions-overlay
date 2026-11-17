"""Read the temporary PostFightStatsHelper capture retained by the game.

This diagnostic reads memory only. It does not install the capture probe.
"""
from __future__ import annotations

import argparse
import ctypes as ct
import math
from pathlib import Path
import time

import fight_report_export as game
import export_complete_fight_report as complete
from export_complete_fight_report import MemoryBasicInformation, READABLE, read_chunk

START = b"UF6R1\n"
END = b"UF6R1_END\n"
MAX_RECORD = 256 * 1024
CHUNK = 4 * 1024 * 1024


def decode_record(data: bytes) -> dict:
    if not data.startswith(START):
        raise ValueError("Capture start is missing")
    stop = data.find(END, len(START))
    if stop < 0:
        raise ValueError("Capture end is missing")
    root: dict = {}
    for line in data[len(START):stop].splitlines():
        path, separator, value = line.partition(b"\t")
        if not separator or not path.startswith(b"stats."):
            raise ValueError("Malformed capture field")
        keys = path.decode("ascii").split(".")[1:]
        if not keys or len(keys) > 9:
            raise ValueError("Invalid capture path")
        node = root
        for key in keys[:-1]:
            node = node.setdefault(key, {})
            if not isinstance(node, dict):
                raise ValueError("Conflicting capture path")
        raw = value.decode("utf-8", "replace")
        if raw == "@empty":
            parsed: object = []
        elif raw in ("true", "false"):
            parsed: object = raw == "true"
        else:
            try:
                number = float(raw)
                parsed = int(number) if math.isfinite(number) and number.is_integer() else number
            except ValueError:
                parsed = raw
        node[keys[-1]] = parsed
    def arrays(value):
        if not isinstance(value, dict):
            return value
        converted = {key: arrays(item) for key, item in value.items()}
        if converted and all(key.isdecimal() for key in converted):
            indexes = sorted(int(key) for key in converted)
            if indexes == list(range(1, len(indexes) + 1)):
                return [converted[str(index)] for index in indexes]
        return converted

    root = arrays(root)
    for corner in ("RED_CORNER_FIGHTER", "BLUE_CORNER_FIGHTER"):
        fighter = root.get(corner)
        if not isinstance(fighter, dict) or not isinstance(fighter.get("TOTAL"), dict):
            raise ValueError(f"{corner} totals are missing")
        rounds = fighter.get("ROUNDS")
        if not isinstance(rounds, list) or not 1 <= len(rounds) <= 12:
            raise ValueError(f"{corner} rounds are missing")
    return root


def read_capture(pid: int, seconds: float = 12.0) -> dict:
    handle = game.kernel.OpenProcess(0x1010, 0, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "Cannot read UFC 6 process")
    info = MemoryBasicInformation()
    address = 0x10000
    deadline = time.monotonic() + seconds
    try:
        while address < 0x7FFF00000000 and time.monotonic() < deadline:
            if not game.kernel.VirtualQueryEx(handle, ct.c_void_p(address),
                                              ct.byref(info), ct.sizeof(info)):
                break
            base = int(info.BaseAddress or 0)
            size = int(info.RegionSize)
            address = base + max(size, 0x1000)
            if info.State != 0x1000 or info.Protect & 0xFF not in READABLE or info.Protect & 0x100:
                continue
            for offset in range(0, size, CHUNK):
                if time.monotonic() >= deadline:
                    break
                length = min(CHUNK, size - offset)
                data = read_chunk(handle, base + offset, length)
                if not data:
                    continue
                pos = data.find(START)
                while pos >= 0:
                    capture = read_chunk(handle, base + offset + pos, min(MAX_RECORD, size - offset - pos))
                    if capture:
                        try:
                            return decode_record(capture)
                        except ValueError:
                            pass
                    pos = data.find(START, pos + 1)
    finally:
        game.kernel.CloseHandle(handle)
    raise RuntimeError("No retained post-fight capture in this game session")


def build_complete_report(pid: int, engine: str, stats: dict, version: str) -> dict:
    handle = game.kernel.OpenProcess(0x1010, 0, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "Cannot read UFC 6 fight state")
    try:
        before = game.snapshot(handle)
        time.sleep(0.12)
        after = game.snapshot(handle)
    finally:
        game.kernel.CloseHandle(handle)
    if before != after:
        raise RuntimeError("Fight data changed during capture")
    fields = after[1]
    if not complete.matches_current_fight({"Success": True, "Output": stats}, fields):
        raise RuntimeError("Retained statistics do not match the current finished fight")
    class Args:
        game_version = version
        red_name = None
        blue_name = None
    report = game.build_report(fields, engine, Args)
    report["fightStatistics"] = stats
    report["fightStatisticsComplete"] = True
    report["fightStatisticsFormat"] = "UFC 6 post-fight UI source, cumulative rounds"
    for corner, fighter in (("RED_CORNER_FIGHTER", report["fighters"][0]),
                            ("BLUE_CORNER_FIGHTER", report["fighters"][1])):
        name = stats[corner].get("NAME")
        if isinstance(name, str) and name.strip():
            fighter["name"] = name.strip()
    return report


if __name__ == "__main__":
    cli = argparse.ArgumentParser()
    cli.add_argument("--seconds", type=float, default=12.0)
    cli.add_argument("--game-version", default="unknown")
    cli.add_argument("--output", type=Path)
    args = cli.parse_args()
    pid, engine = game.game_process()
    stats = read_capture(pid, args.seconds)
    report = build_complete_report(pid, engine, stats, args.game_version)
    print(complete.save_report(report, args.output))

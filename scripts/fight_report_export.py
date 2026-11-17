"""Export a read-only UFC 6 post-fight report for manual Parsectagon import.

Run while Fight Statistics is open. The file contains no lobby credentials and
does not submit a ranked result. Both lobby fighters still report separately.
"""

import argparse
import ctypes
import json
import math
import struct
import time
import uuid
from pathlib import Path

import psutil


SLOTS = {
    "Global_MatchState": 0x90B444C10,
    "Global_MatchResultType": 0x90B444BE8,
    "Global_RoundNum": 0x90B444F08,
    "Global_RoundLength": 0x90B444EE0,
    "Online_MatchResult": 0x90B46A820,
    "Online_MatchResultType": 0x90B46A870,
    "Online_MatchTimeSecs": 0x90B46A8C0,
    "Online_KnockdownsLanded": 0x90B46A988,
    # The names of these four game-report fields are misleading. Their values
    # were compared against the game's Overall Fight Statistics screen.
    "Online_AdvanceStrikesLanded": 0x90B46AC30,
    "Online_SignificantStrikesThrown": 0x90B46A960,
    "Online_StrikesLanded": 0x90B46A9B0,
    "Online_StrikesThrown": 0x90B46A9D8,
    "Online_TakedownsAttempted": 0x90B46AA78,
    "Online_TakedownsLanded": 0x90B46AAA0,
    "Online_StunsLanded": 0x90B46AC80,
    "Online_Control_Duration": 0x90B46ACA8,
}

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
kernel.OpenProcess.restype = ctypes.c_void_p
kernel.ReadProcessMemory.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
    ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t),
]
kernel.ReadProcessMemory.restype = ctypes.c_int
kernel.CloseHandle.argtypes = [ctypes.c_void_p]


def game_process():
    names = {"UFC 6 novo.exe": "ps5-pro", "UFC 6 Parsec.exe": "original-parsec"}
    matches = [(p.pid, names[p.info["name"]]) for p in psutil.process_iter(["name"])
               if p.info["name"] in names]
    if len(matches) != 1:
        raise RuntimeError("Start exactly one UFC 6 game engine before exporting.")
    return matches[0]


def read_exact(handle, address, length):
    if address < 0x10000 or length > 128:
        raise RuntimeError("Invalid game-report address.")
    buffer = ctypes.create_string_buffer(length)
    received = ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, ctypes.c_void_p(address), buffer,
                                    length, ctypes.byref(received)) or received.value != length:
        raise RuntimeError("Game-report memory is unavailable.")
    return buffer.raw


def u64(handle, address):
    return struct.unpack("<Q", read_exact(handle, address, 8))[0]


def read_field(handle, name, expected_world):
    obj = u64(handle, SLOTS[name])
    header = read_exact(handle, obj, 0x68)
    if struct.unpack_from("<Q", header)[0] != 0x909C227F0:
        raise RuntimeError(f"{name}: game-report layout changed.")
    field_name = read_exact(handle, struct.unpack_from("<Q", header, 8)[0], 96)
    if field_name.split(b"\0", 1)[0].decode("ascii", "replace") != name:
        raise RuntimeError(f"{name}: game-report name mismatch.")
    world = struct.unpack_from("<Q", header, 0x28)[0]
    if expected_world and world != expected_world:
        raise RuntimeError("Fight state changed during capture. Try again.")
    if struct.unpack_from("<I", header, 0x58)[0] != 4:
        raise RuntimeError(f"{name}: unexpected value size.")
    values = read_exact(handle, struct.unpack_from("<Q", header, 0x60)[0], 40)
    if struct.unpack_from("<H", values, 0x1C)[0] != 32:
        raise RuntimeError(f"{name}: unexpected value layout.")
    return world, struct.unpack_from("<II", values, 0x20)


def snapshot(handle):
    world = 0
    fields = {}
    for name in SLOTS:
        world, fields[name] = read_field(handle, name, world)
    return world, fields


def count(fields, name, index):
    value = fields[name][index]
    if value > 9999:
        raise RuntimeError(f"{name}: value is outside the expected range.")
    return value


def fighter_stats(fields, index):
    stats = {
        "knockdowns": count(fields, "Online_KnockdownsLanded", index),
        "stuns": count(fields, "Online_StunsLanded", index),
        "significantStrikesAttempted": count(fields, "Online_AdvanceStrikesLanded", index),
        "significantStrikesLanded": count(fields, "Online_SignificantStrikesThrown", index),
        "totalStrikesAttempted": count(fields, "Online_StrikesLanded", index),
        "totalStrikesLanded": count(fields, "Online_StrikesThrown", index),
        "takedownsAttempted": count(fields, "Online_TakedownsAttempted", index),
        "takedownsLanded": count(fields, "Online_TakedownsLanded", index),
    }
    for prefix in ("significantStrikes", "totalStrikes", "takedowns"):
        if stats[prefix + "Landed"] > stats[prefix + "Attempted"]:
            raise RuntimeError(f"{prefix}: game report is inconsistent.")
    seconds = struct.unpack("<f", struct.pack("<I", fields["Online_Control_Duration"][index]))[0]
    if math.isfinite(seconds) and 0 <= seconds <= 9999:
        stats["controlTimeSeconds"] = round(seconds)
    return stats


def build_report(fields, engine, args):
    if fields["Global_MatchState"][0] != 3:
        raise RuntimeError("The game is not showing a finished fight.")
    if fields["Global_MatchResultType"][0] != fields["Online_MatchResultType"][0]:
        raise RuntimeError("The game report does not match the current fight.")
    # Game fighter index 0 is blue and index 1 is red in the validated build.
    results = fields["Online_MatchResult"]
    if results == (2, 1):
        winner = "red"
    elif results == (1, 2):
        winner = "blue"
    else:
        raise RuntimeError("A finished win/loss result is not available. Keep Fight Statistics open.")
    round_number = fields["Global_RoundNum"][0] + 1
    if not 1 <= round_number <= 12:
        raise RuntimeError("The finished round could not be verified.")
    round_length = struct.unpack("<f", struct.pack("<I", fields["Global_RoundLength"][0]))[0]
    elapsed = fields["Online_MatchTimeSecs"][0]
    if not math.isfinite(round_length) or not 30 <= round_length <= 1800:
        raise RuntimeError("The fight's round length could not be verified.")
    remaining_seconds = elapsed - (round_number - 1) * round_length
    if not 0 <= remaining_seconds <= round_length:
        raise RuntimeError("The finish time does not match the finished round.")
    clock_seconds = round(round_length - remaining_seconds)
    # These codes match the game's PostFightStatsHelper result enum; TKO (1)
    # was also checked against a live Fight Statistics result.
    method = {0: "Knockout", 1: "TKO", 2: "Submission"}.get(
        fields["Global_MatchResultType"][0])
    return {
        "schema": 1,
        "id": str(uuid.uuid4()),
        "gameVersion": args.game_version,
        "engine": engine,
        "capturedAt": int(time.time() * 1000),
        "result": {"winnerCorner": winner, "method": method,
                   "gameMethodCode": fields["Global_MatchResultType"][0],
                   "round": round_number, "clockSeconds": clock_seconds},
        "fighters": [
            {"corner": "red", "name": args.red_name, "stats": fighter_stats(fields, 1)},
            {"corner": "blue", "name": args.blue_name, "stats": fighter_stats(fields, 0)},
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--red-name", default=None)
    parser.add_argument("--blue-name", default=None)
    parser.add_argument("--game-version", default="unknown")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    pid, engine = game_process()
    handle = kernel.OpenProcess(0x410, 0, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), "Cannot read UFC 6.")
    try:
        before = snapshot(handle)
        time.sleep(0.15)
        after = snapshot(handle)
    finally:
        kernel.CloseHandle(handle)
    if before != after:
        raise RuntimeError("Fight data changed during capture. Try again.")
    report = build_report(after[1], engine, args)
    destination = args.output or (Path.home() / "Documents" / "Parsectagon" /
                                  "Fight Reports" / f"fight-{time.strftime('%Y%m%d-%H%M%S')}-{report['id'][:8]}.json")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(destination)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as error:
        raise SystemExit(f"Fight report not exported: {error}")

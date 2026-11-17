"""Capture the complete Fight Statistics service record from a finished UFC 6 fight.

Read-only access to game memory. The output is local and is never submitted to
Parsectagon. The optional address is for a diagnosed session; normal use scans
readable memory for the service response on a worker process.
"""
from __future__ import annotations

import argparse
import os
import ctypes as ct
from ctypes import wintypes as wt
import json
from pathlib import Path
import time

import fight_report_export as result_export
from decode_fight_stats_blob import decode, read


PREFIX = b"\x08Success\x00\x02\x01\x08Output\x00\x01"
READABLE = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}
CHUNK = 1 << 20
RESPONSE_LIMIT = 1 << 18


class MemoryBasicInformation(ct.Structure):
    _fields_ = [
        ("BaseAddress", ct.c_void_p),
        ("AllocationBase", ct.c_void_p),
        ("AllocationProtect", wt.DWORD),
        ("PartitionId", wt.WORD),
        ("RegionSize", ct.c_size_t),
        ("State", wt.DWORD),
        ("Protect", wt.DWORD),
        ("Type", wt.DWORD),
    ]


result_export.kernel.VirtualQueryEx.argtypes = (
    ct.c_void_p, ct.c_void_p, ct.POINTER(MemoryBasicInformation), ct.c_size_t
)
result_export.kernel.VirtualQueryEx.restype = ct.c_size_t


def read_chunk(handle: int, address: int, size: int) -> bytes | None:
    data = ct.create_string_buffer(size)
    count = ct.c_size_t()
    if result_export.kernel.ReadProcessMemory(
        handle, ct.c_void_p(address), data, size, ct.byref(count)
    ) and count.value == size:
        return data.raw
    return None


def matches_current_fight(response: dict, fields: dict) -> bool:
    if response.get("Success") is not True:
        return False
    output = response.get("Output")
    if not isinstance(output, dict):
        return False
    try:
        if int(output["ROUND_NUMBER"]) != fields["Global_RoundNum"][0] + 1:
            return False
        rounds = []
        for corner, index in (("RED_CORNER_FIGHTER", 1), ("BLUE_CORNER_FIGHTER", 0)):
            fighter = output[corner]
            total = fighter["TOTAL"]
            rounds.append(fighter["ROUNDS"])
            if int(total["KNOCKDOWNS_LANDED"]) != fields["Online_KnockdownsLanded"][index]:
                return False
            if int(total["SIGNIFICANT_STRIKES_THROWN"]) != fields["Online_SignificantStrikesThrown"][index]:
                return False
            if int(total["SIGNIFICANT_STRIKES_LANDED"]) != fields["Online_AdvanceStrikesLanded"][index]:
                return False
        if not 1 <= len(rounds[0]) == len(rounds[1]) <= 12:
            return False
    except (KeyError, IndexError, TypeError, ValueError, OverflowError):
        return False
    return True


def matches_fighter_profiles(response: dict, _fields: dict) -> bool:
    if response.get("Success") is not True or not isinstance(response.get("Output"), dict):
        return False
    try:
        fighters = [response["Output"][corner] for corner in
                    ("RED_CORNER_FIGHTER", "BLUE_CORNER_FIGHTER")]
        if any("ROUNDS" in fighter or not isinstance(fighter.get("NAME"), str) or
               not 1 <= len(fighter["NAME"].strip()) <= 64 or
               not isinstance(fighter.get("ID"), (int, float)) or
               not 1 <= fighter["ID"] <= 100000 or fighter["ID"] % 1
               for fighter in fighters):
            return False
        return fighters[0]["ID"] != fighters[1]["ID"]
    except (KeyError, TypeError, ValueError):
        return False


def matches_overall_fight(response: dict, fields: dict) -> bool:
    """Check the retained result response against the live finished fight."""
    try:
        result = response["Output"]["RESULTS"]
        fighters = result["FIGHTER_STATS"]
        if not response.get("Success") or len(fighters) != 2 or \
           int(result["ROUND_NUMBER"]) != fields["Global_RoundNum"][0] + 1 or \
           int(result["RESULT_TYPE"]) != fields["Global_MatchResultType"][0]:
            return False
        for index, fighter in enumerate(fighters):
            if int(fighter["KNOCKDOWNS_LANDED"]) != fields["Online_KnockdownsLanded"][index] or \
               int(fighter["SIGNIFICANT_STRIKES_THROWN"]) != fields["Online_SignificantStrikesThrown"][index] or \
               int(fighter["SIGNIFICANT_STRIKES_LANDED"]) != fields["Online_AdvanceStrikesLanded"][index]:
                return False
        return True
    except (KeyError, IndexError, TypeError, ValueError, OverflowError):
        return False


def parse_candidate(pid: int, address: int, fields: dict, matcher=None) -> tuple[dict, int] | None:
    matcher = matcher or matches_current_fight
    try:
        response, consumed = decode(read(pid, address, RESPONSE_LIMIT))
    except (OSError, ValueError):
        return None
    if consumed > 0 and matcher(response, fields):
        return response, consumed
    return None


def find_response(pid: int, fields: dict, deadline: float, matcher=None) -> tuple[dict, int]:
    handle = result_export.kernel.OpenProcess(0x1010, 0, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "Cannot scan UFC 6 memory")
    info = MemoryBasicInformation()
    address = 0x10000
    checked = set()
    try:
        while address < 0x7FFF00000000 and time.monotonic() < deadline:
            if not result_export.kernel.VirtualQueryEx(
                handle, ct.c_void_p(address), ct.byref(info), ct.sizeof(info)
            ):
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
                start = base + offset
                data = read_chunk(handle, start, length)
                if data is None:
                    continue
                pos = data.find(PREFIX)
                while pos >= 0:
                    candidate = start + pos
                    if candidate not in checked:
                        checked.add(candidate)
                        result = parse_candidate(pid, candidate, fields, matcher)
                        if result:
                            return result
                    pos = data.find(PREFIX, pos + 1)
                if offset + length < size:
                    boundary = start + length - len(PREFIX) + 1
                    tail = read_chunk(handle, boundary, len(PREFIX) * 2)
                    if tail:
                        pos = tail.find(PREFIX)
                        if pos >= 0:
                            candidate = boundary + pos
                            if candidate not in checked:
                                checked.add(candidate)
                                result = parse_candidate(pid, candidate, fields, matcher)
                                if result:
                                    return result
    finally:
        result_export.kernel.CloseHandle(handle)
    raise RuntimeError("The game has not exposed a complete Fight Statistics record for this fight")


def capture(address: int | None, deadline_seconds: float, game_version: str = "unknown",
            red_name: str | None = None, blue_name: str | None = None) -> dict:
    pid, engine = result_export.game_process()
    handle = result_export.kernel.OpenProcess(0x1010, 0, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "Cannot read UFC 6")
    try:
        before = result_export.snapshot(handle)
        time.sleep(0.12)
        after = result_export.snapshot(handle)
    finally:
        result_export.kernel.CloseHandle(handle)
    if before != after:
        raise RuntimeError("Fight data changed during capture")
    fields = after[1]
    class Args:
        pass
    Args.game_version = game_version
    Args.red_name = red_name
    Args.blue_name = blue_name
    report = result_export.build_report(fields, engine, Args)
    fallback = None
    profile = None
    def match_and_collect(response, live_fields):
        nonlocal fallback, profile
        if matches_fighter_profiles(response, live_fields):
            profile = response
        if matches_overall_fight(response, live_fields):
            fallback = response
        return matches_current_fight(response, live_fields)

    if address is not None:
        found = parse_candidate(pid, address, fields)
        if found is None:
            raise RuntimeError("Specified Fight Statistics record is incomplete or belongs to another fight")
    else:
        try:
            found = find_response(pid, fields, time.monotonic() + deadline_seconds, match_and_collect)
        except RuntimeError:
            if fallback is None:
                raise
            found = None
    if found is not None:
        response, consumed = found
        report["fightStatistics"] = response["Output"]
        report["fightStatisticsComplete"] = True
        report["fightStatisticsFormat"] = "UFC 6 get-fight-stats response, cumulative rounds"
        report["fightStatisticsBytes"] = consumed
    else:
        result = fallback["Output"]["RESULTS"]
        report["overallStatistics"] = {
            "result": {key: value for key, value in result.items() if key != "FIGHTER_STATS"},
            "fighters": {"red": result["FIGHTER_STATS"][1],
                         "blue": result["FIGHTER_STATS"][0]},
        }
        report["fightStatisticsComplete"] = False
        report["fightStatisticsFormat"] = "UFC 6 match-result response, overall stats only; round records unavailable"
    if (red_name is None or blue_name is None) and profile is None:
        try:
            profile, _ = find_response(pid, fields, time.monotonic() + 2, matches_fighter_profiles)
        except RuntimeError:
            pass  # The report remains useful when fighter profiles have expired.
    if profile is not None:
        for corner, fighter in (("RED_CORNER_FIGHTER", report["fighters"][0]),
                                ("BLUE_CORNER_FIGHTER", report["fighters"][1])):
            if fighter["name"] is None:
                fighter["name"] = profile["Output"][corner]["NAME"].strip()
            fighter["gameFighterId"] = int(profile["Output"][corner]["ID"])
    return report


def documents_folder() -> Path:
    """Use the same Windows known folder as the native result-first capture."""
    class Guid(ct.Structure):
        _fields_ = [("data1", wt.DWORD), ("data2", wt.WORD),
                    ("data3", wt.WORD), ("data4", ct.c_ubyte * 8)]

    folder_id = Guid(0xfdd39ad0, 0x238f, 0x46af,
                     (ct.c_ubyte * 8)(0xad, 0xb4, 0x6c, 0x85, 0x48, 0x03, 0x69, 0xc7))
    location = ct.c_void_p()
    if ct.windll.shell32.SHGetKnownFolderPath(ct.byref(folder_id), 0, None,
                                               ct.byref(location)) != 0 or not location.value:
        raise OSError("Windows Documents folder is unavailable")
    try:
        return Path(ct.wstring_at(location.value))
    finally:
        ct.windll.ole32.CoTaskMemFree(location)


def save_report(report: dict, destination: Path | None = None) -> Path:
    if destination is None:
        destination = (documents_folder() / "Parsectagon" / "Fight Reports" /
                       f"fight-complete-{report['id'][:8]}.json")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return destination


def main() -> None:
    arg = argparse.ArgumentParser(description=__doc__)
    arg.add_argument("--address", type=lambda s: int(s, 0))
    arg.add_argument("--scan-seconds", type=float, default=120)
    arg.add_argument("--output", type=Path)
    arg.add_argument("--game-version", default="unknown")
    arg.add_argument("--red-name")
    arg.add_argument("--blue-name")
    args = arg.parse_args()
    if not 1 <= args.scan_seconds <= 300:
        raise ValueError("Scan timeout must be between 1 and 300 seconds")
    report = capture(args.address, args.scan_seconds, args.game_version, args.red_name, args.blue_name)
    destination = save_report(report, args.output)
    print(destination)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        raise SystemExit(f"Complete fight report not exported: {error}")

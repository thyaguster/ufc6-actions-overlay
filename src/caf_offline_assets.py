"""Reversible, exact-byte test of UFC 6's CAF offline popup route."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vendor" / "xbox-icons"))
import patcher_core as core

OFFSET = 0x26241312
LENGTH = 762
ORIGINAL_SHA256 = "dd0cc6e08879100776be92513d8a14a87c05127350f5b40ea43e8bdbf60cb405"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replacement(original):
    obj = json.loads(original)
    assert obj["name"] == "UserAgreementPopupChecker"
    assert len(obj["states"]) == 1
    state = obj["states"][0]
    assert state["name"] == "ShowCafOfflinePopUp"
    assert state["onEnter"] == [
        ["set", ["popup", {}]],
        ["openPopup", ["/features/common/cafoffline.pop", ".popup"]],
    ]
    assert state["onExit"] == [["closePopup", [".popup"]]]
    assert state["transitions"] == [
        {"event": "evt_caf_offline_accept", "targets": ["out_caf_offline_confirmed"]}
    ]
    state["onEnter"] = [["invoke", ["evt_caf_offline_accept"]]]
    state["onExit"] = []
    encoded = json.dumps(obj, separators=(",", ":")).encode("ascii")
    assert len(encoded) <= LENGTH
    return encoded + b" " * (LENGTH - len(encoded))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["status", "install", "restore"])
    parser.add_argument("archive", type=Path)
    parser.add_argument("backup", type=Path)
    args = parser.parse_args()
    if args.action != "status":
        core.check(not core.game_running(), "Close UFC 6 before changing the career offline popup.")
    with args.archive.open("rb") as stream:
        stream.seek(OFFSET)
        current = stream.read(LENGTH)
    if args.backup.exists():
        original = args.backup.read_bytes()
        assert len(original) == LENGTH and digest(original) == ORIGINAL_SHA256
    else:
        assert digest(current) == ORIGINAL_SHA256, "Unknown archive bytes; no backup or change"
        original = current
    patched = replacement(original)
    assert current in (original, patched), "Archive has changed; refusing to overwrite"
    if args.action == "status":
        print("patched" if current == patched else "original")
        return
    if args.action == "install" and not args.backup.exists():
        with args.backup.open("xb") as stream:
            stream.write(original)
            stream.flush()
            os.fsync(stream.fileno())
    desired = patched if args.action == "install" else original
    if current != desired:
        with args.archive.open("r+b") as stream:
            stream.seek(OFFSET)
            stream.write(desired)
            stream.flush()
            os.fsync(stream.fileno())
        with args.archive.open("rb") as stream:
            stream.seek(OFFSET)
            assert stream.read(LENGTH) == desired
    print("patched" if desired == patched else "original")


if __name__ == "__main__":
    main()

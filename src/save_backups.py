"""Verified, read-only copies of UFC 6 saves; no restore or game-memory writes."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import stat
import tempfile
import uuid
import zipfile

TITLE = "PPSA23566"
MAX_FILES = 10000
MAX_BYTES = 2 * 1024**3


def regular(path):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError("Save folders contain a link or junction; no backup was created.")
    return info


def inventory(root):
    regular(root)
    trees = [root / TITLE] if (root / TITLE).exists() else []
    users = root / "[users]"
    if users.exists():
        regular(users)
        for user in users.iterdir():
            info = regular(user)
            if stat.S_ISDIR(info.st_mode) and (user / TITLE).exists():
                trees.append(user / TITLE)
    found = {}
    total = 0
    def walk(folder):
        nonlocal total
        if not stat.S_ISDIR(regular(folder).st_mode):
            raise ValueError("Invalid save folder.")
        for path in sorted(folder.iterdir()):
            info = regular(path)
            if stat.S_ISDIR(info.st_mode):
                walk(path)
            elif stat.S_ISREG(info.st_mode):
                name = path.relative_to(root).as_posix()
                found[name] = (info.st_size, info.st_mtime_ns, info.st_ino)
                total += info.st_size
                if len(found) > MAX_FILES or total > MAX_BYTES:
                    raise ValueError("Save backup exceeds the supported size limit.")
            else:
                raise ValueError("Unsupported file in save folder.")
    for tree in trees:
        walk(tree)
    if not found:
        raise ValueError("No UFC 6 saves found in this session's _SaveData folder.")
    return found


def create_backup(save_root, destination):
    root = Path(save_root).absolute()
    before = inventory(root)
    destination = Path(destination).absolute()
    if destination.resolve().is_relative_to(root.resolve()):
        raise ValueError("Backups must be stored outside the save folder.")
    destination.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc)
    final = destination / (now.strftime("UFC6-saves-%Y%m%d-%H%M%S-") + uuid.uuid4().hex[:8] + ".zip")
    descriptor, temp_name = tempfile.mkstemp(prefix=".ufc6-backup-", suffix=".partial", dir=destination)
    os.close(descriptor)
    temp = Path(temp_name)
    hashes = {}
    try:
        with zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as archive:
            for name in sorted(before):
                digest = hashlib.sha256()
                with (root / name).open("rb") as source, archive.open("_SaveData/" + name, "w", force_zip64=True) as output:
                    while chunk := source.read(1024 * 1024):
                        digest.update(chunk)
                        output.write(chunk)
                hashes[name] = digest.hexdigest()
            if inventory(root) != before:
                raise ValueError("Saves changed during backup. Wait for autosave to finish and retry.")
            for name, expected in hashes.items():
                digest = hashlib.sha256()
                with (root / name).open("rb") as source:
                    while chunk := source.read(1024 * 1024):
                        digest.update(chunk)
                if digest.hexdigest() != expected:
                    raise ValueError("Saves changed during backup. Wait for autosave to finish and retry.")
            if inventory(root) != before:
                raise ValueError("Saves changed during backup. Wait for autosave to finish and retry.")
            manifest = {"format": 1, "title_id": TITLE, "created_utc": now.isoformat(),
                        "files": {"_SaveData/" + name: {"size": before[name][0], "sha256": digest} for name, digest in hashes.items()}}
            archive.writestr("backup-manifest.json", json.dumps(manifest, indent=2) + "\n")
        with zipfile.ZipFile(temp) as archive:
            for name, expected in hashes.items():
                digest = hashlib.sha256()
                with archive.open("_SaveData/" + name) as stored:
                    while chunk := stored.read(1024 * 1024):
                        digest.update(chunk)
                if digest.hexdigest() != expected:
                    raise ValueError("Backup verification failed; no completed ZIP was kept.")
        if final.exists():
            raise FileExistsError("Backup filename already exists; existing backup was kept.")
        os.rename(temp, final)
        return final, len(hashes)
    finally:
        temp.unlink(missing_ok=True)

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location("save_backups", Path(__file__).resolve().parents[1] / "src/save_backups.py")
backup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(backup)


class BackupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "_SaveData"
        self.file = self.root / "PPSA23566" / "career" / "save.bin"
        self.file.parent.mkdir(parents=True)
        self.file.write_bytes(bytes(range(256)) * 100)
        self.dest = Path(self.temp.name) / "backups"

    def test_roundtrip_all_users_and_manifest_without_other_titles(self):
        guest = self.root / "[users]" / "42" / "PPSA23566" / "settings.bin"
        guest.parent.mkdir(parents=True)
        guest.write_bytes(b"guest settings")
        other = self.root / "OTHERGAME" / "save.bin"
        other.parent.mkdir()
        other.write_bytes(b"not UFC 6")
        before = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob("*") if p.is_file()}
        archive, count = backup.create_backup(self.root, self.dest)
        self.assertEqual(count, 2)
        with zipfile.ZipFile(archive) as z:
            manifest = json.loads(z.read("backup-manifest.json"))
            self.assertEqual(manifest["title_id"], "PPSA23566")
            for name, record in manifest["files"].items():
                content = z.read(name)
                self.assertEqual(content, (self.root.parent / name).read_bytes())
                self.assertEqual(hashlib.sha256(content).hexdigest(), record["sha256"])
                self.assertEqual(len(content), record["size"])
            self.assertFalse(any("OTHERGAME" in n for n in z.namelist()))
        for p, (content, mtime) in before.items():
            self.assertEqual(p.read_bytes(), content)
            self.assertEqual(p.stat().st_mtime_ns, mtime)

    def test_multiple_backups_preserve_existing_archive(self):
        first, _ = backup.create_backup(self.root, self.dest)
        content = first.read_bytes()
        second, _ = backup.create_backup(self.root, self.dest)
        self.assertNotEqual(first, second)
        self.assertEqual(first.read_bytes(), content)

    def test_missing_saves_never_creates_destination(self):
        self.file.unlink()
        with self.assertRaisesRegex(ValueError, "No UFC 6 saves"):
            backup.create_backup(self.root, self.dest)
        self.assertFalse(self.dest.exists())

    def test_destination_inside_saves_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            backup.create_backup(self.root, self.root / "backups")
        self.assertFalse((self.root / "backups").exists())

    def test_changing_save_discards_partial_archive(self):
        original = backup.inventory
        calls = 0
        def changing(root):
            nonlocal calls
            calls += 1
            if calls == 2:
                self.file.write_bytes(b"new autosave")
            return original(root)
        with patch.object(backup, "inventory", changing):
            with self.assertRaisesRegex(ValueError, "changed during backup"):
                backup.create_backup(self.root, self.dest)
        self.assertEqual(list(self.dest.iterdir()), [])
        self.assertEqual(self.file.read_bytes(), b"new autosave")

    def test_io_failure_discards_partial_and_preserves_old_backups(self):
        self.dest.mkdir()
        old = self.dest / "old.zip"
        old.write_bytes(b"existing backup")
        with patch.object(backup.zipfile.ZipFile, "open", side_effect=OSError("disk full")):
            with self.assertRaises(OSError):
                backup.create_backup(self.root, self.dest)
        self.assertEqual(list(self.dest.iterdir()), [old])
        self.assertEqual(old.read_bytes(), b"existing backup")

    def test_file_limit_rejects_before_creating_destination(self):
        with patch.object(backup, "MAX_FILES", 0):
            with self.assertRaisesRegex(ValueError, "size limit"):
                backup.create_backup(self.root, self.dest)
        self.assertFalse(self.dest.exists())

    def test_reparse_point_is_rejected(self):
        actual = self.file.lstat()
        class Reparse:
            st_mode = actual.st_mode
            st_file_attributes = 0x400
        with patch.object(Path, "lstat", return_value=Reparse()):
            with self.assertRaisesRegex(ValueError, "link or junction"):
                backup.regular(self.file)


if __name__ == "__main__":
    unittest.main()

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import manage_menu


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=manage_menu.ROOT / ".build")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.engine = self.bin / "UFC 6.exe"
        self.engine.write_bytes(b"fixture engine: never modified")
        self.payload = self.root / "payload.dll"
        self.payload.write_bytes(b"fixture menu")
        self.manifest = self.root / "release.json"
        self.manifest.write_text(json.dumps({"dll_sha256": manage_menu.digest(self.payload)}))
        self.patch = patch.object(manage_menu, "ENGINE_SHA256", manage_menu.digest(self.engine))
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def run_action(self, action):
        return manage_menu.manage(action, self.bin, self.payload, self.manifest)

    def test_roundtrip_and_idempotence(self):
        before = self.engine.read_bytes()
        self.assertEqual(self.run_action("status")["menu"], "absent")
        for _ in range(2):
            self.assertEqual(self.run_action("install")["menu"], "installed")
        self.assertEqual((self.bin / "version.dll").read_bytes(), self.payload.read_bytes())
        for _ in range(2):
            self.assertEqual(self.run_action("remove")["menu"], "absent")
        self.assertEqual(self.engine.read_bytes(), before)

    def test_conflict_never_overwritten_or_removed(self):
        target = self.bin / "version.dll"
        target.write_bytes(b"another mod")
        for action in ["install", "remove"]:
            with self.assertRaisesRegex(RuntimeError, "different version.dll"):
                self.run_action(action)
            self.assertEqual(target.read_bytes(), b"another mod")

    def test_unknown_engine_rejected(self):
        self.engine.write_bytes(b"different engine")
        with self.assertRaisesRegex(RuntimeError, "Engine hash"):
            self.run_action("install")
        self.assertFalse((self.bin / "version.dll").exists())

    def test_bad_payload_rejected(self):
        self.payload.write_bytes(b"corrupt menu")
        with self.assertRaisesRegex(RuntimeError, "Menu DLL"):
            self.run_action("install")
        self.assertFalse((self.bin / "version.dll").exists())

    def test_known_previous_build_upgrade(self):
        previous = b"previous menu build"
        previous_hash = hashlib.sha256(previous).hexdigest()
        manifest = json.loads(self.manifest.read_text())
        manifest["previous_dll_sha256"] = [previous_hash]
        self.manifest.write_text(json.dumps(manifest))
        (self.bin / "version.dll").write_bytes(previous)
        self.assertEqual(self.run_action("status")["menu"], "installed (older build)")
        result = self.run_action("install")
        self.assertEqual(result["menu"], "installed")
        self.assertEqual(Path(result["previous_dll_backup"]).read_bytes(), previous)
        self.assertEqual((self.bin / "version.dll").read_bytes(), self.payload.read_bytes())

    def add_companion(self):
        path = self.root / "ufc6_menu_layer.dll"
        path.write_bytes(b"fixture Vulkan layer")
        manifest = json.loads(self.manifest.read_text())
        manifest["companion_files"] = {path.name: manage_menu.digest(path)}
        self.manifest.write_text(json.dumps(manifest))
        return path

    def test_companion_install_remove(self):
        layer = self.add_companion()
        self.run_action("install")
        self.assertEqual((self.bin / layer.name).read_bytes(), layer.read_bytes())
        self.run_action("remove")
        self.assertFalse((self.bin / layer.name).exists())

    def test_companion_conflict_prevents_any_write(self):
        layer = self.add_companion()
        (self.bin / layer.name).write_bytes(b"someone else's file")
        with self.assertRaisesRegex(RuntimeError, "unknown file"):
            self.run_action("install")
        self.assertFalse((self.bin / "version.dll").exists())
        self.assertEqual((self.bin / layer.name).read_bytes(), b"someone else's file")

    def test_corrupt_companion_prevents_any_write(self):
        layer = self.add_companion()
        layer.write_bytes(b"corrupt layer")
        with self.assertRaisesRegex(RuntimeError, "hash mismatch"):
            self.run_action("install")
        self.assertFalse((self.bin / "version.dll").exists())
        self.assertFalse((self.bin / layer.name).exists())

    def test_known_companion_upgrade_and_transaction_rollback(self):
        layer=self.add_companion()
        old_layer=b"old Vulkan layer"
        old_core=b"old menu core"
        (self.bin/layer.name).write_bytes(old_layer)
        (self.bin/"version.dll").write_bytes(old_core)
        manifest=json.loads(self.manifest.read_text())
        manifest["previous_companion_sha256"]={layer.name:[hashlib.sha256(old_layer).hexdigest()]}
        manifest["previous_dll_sha256"]=[hashlib.sha256(old_core).hexdigest()]
        self.manifest.write_text(json.dumps(manifest))
        self.assertEqual(self.run_action("status")["vulkan_files"][layer.name],"installed (older build)")
        rename=Path.rename
        def fail_core_commit(path,target):
            if path.suffix==".tmp" and Path(target).name=="version.dll":
                raise OSError("simulated final core commit failure")
            return rename(path,target)
        with patch.object(Path,"rename",fail_core_commit):
            with self.assertRaisesRegex(OSError,"simulated"):
                self.run_action("install")
        self.assertEqual((self.bin/layer.name).read_bytes(),old_layer)
        self.assertEqual((self.bin/"version.dll").read_bytes(),old_core)
        self.assertFalse(list(self.bin.glob(".ufc6-menu-*.tmp")))
        result=self.run_action("install")
        self.assertEqual(result["menu"],"installed")
        self.assertEqual((self.bin/layer.name).read_bytes(),layer.read_bytes())
        self.assertEqual(Path(result["previous_companion_backups"][layer.name]).read_bytes(),old_layer)

    def test_settings_survive_install_and_remove(self):
        settings=self.bin/"ufc6-quit-menu.ini"
        contents=b"[Menu]\nButtonIcons=PlayStation\nOpeningShortcut=0x6\nUIScale=175\nAutoLaunchAfterRestart=0\n"
        settings.write_bytes(contents)
        self.run_action("install");self.run_action("remove")
        self.assertEqual(settings.read_bytes(),contents)


if __name__ == "__main__":
    unittest.main()

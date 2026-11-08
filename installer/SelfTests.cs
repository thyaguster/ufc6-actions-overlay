// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace UFC6OverlaySetup {
    public static class SelfTests {
        static void Assert(bool condition,string message) { if (!condition) throw new Exception(message); }
        static byte[] Bytes(string text) { return Encoding.UTF8.GetBytes(text); }
        static void Reject(Action action,string message) { bool rejected = false; try { action(); } catch (IOException) { rejected = true; } catch (InvalidDataException) { rejected = true; } Assert(rejected,message); }
        public static void Run(string report) {
            string root = Path.Combine(Path.GetTempPath(),"UFC6 setup tests " + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            var passed = new List<string>();
            try {
                string folder = Path.Combine(root,"game with spaces","bin"); Directory.CreateDirectory(folder);
                string engine = Path.Combine(folder,"UFC 6.exe"); File.WriteAllBytes(engine,Bytes("fixture engine"));
                var payload = new Dictionary<string,byte[]> { {"ufc6_menu_layer.dll",Bytes("new layer")}, {"ufc6_menu_layer.json",Bytes("new manifest")}, {"ufc6_menu_restart.exe",Bytes("new helper")}, {"version.dll",Bytes("new core")} };
                var release = new Release { version = "fixture", engine_sha256 = Manager.HashFile(engine), dll_sha256 = Manager.Hash(payload["version.dll"]), previous_dll_sha256 = new [] {Manager.Hash(Bytes("old core"))}, companion_files = payload.Where(p => p.Key != "version.dll").ToDictionary(p => p.Key,p => Manager.Hash(p.Value)), previous_companion_sha256 = new Dictionary<string,string[]> { {"ufc6_menu_layer.dll",new [] {Manager.Hash(Bytes("old layer"))}} } };
                var manager = new Manager(release,payload);
                Assert(Manager.Normalize(Directory.GetParent(folder).FullName) == folder,"Game root must resolve to bin.");
                Assert(manager.Inspect(folder).Supported,"Fixture engine must be supported.");
                string ini = Path.Combine(folder,"ufc6-quit-menu.ini"), screenshot = Path.Combine(folder,"existing-screenshot.png");
                File.WriteAllText(ini,"[Menu]\nUIScale=175\nAutoLaunchAfterRestart=0\n"); File.WriteAllBytes(screenshot,Bytes("existing image"));
                string settings = File.ReadAllText(ini);
                manager.Apply(folder,false); Assert(manager.Inspect(folder).Current == 4,"Installation must match every release hash.");
                manager.Apply(folder,false); Assert(!Directory.GetFiles(folder,"*.bak").Any(),"Idempotent install must not create backups.");
                manager.Apply(folder,true); Assert(manager.Inspect(folder).Installed == 0,"Remove must uninstall all four files.");
                Assert(File.ReadAllText(ini) == settings && File.ReadAllText(screenshot) == "existing image","User data must survive install and removal.");
                passed.Add("PASS: root/bin selection, spaced paths, install/remove, idempotence, settings and screenshot preservation.");
                File.WriteAllBytes(engine,Bytes("unsupported engine"));
                Reject(() => manager.Apply(folder,false),"Unsupported engine must be rejected.");
                Assert(!File.Exists(Path.Combine(folder,"version.dll")),"Unsupported engine must not install a core.");
                File.WriteAllBytes(engine,Bytes("fixture engine"));
                foreach (string name in manager.Order) {
                    string target = Path.Combine(folder,name); File.WriteAllBytes(target,Bytes("unrelated mod"));
                    Reject(() => manager.Apply(folder,false),"Unknown mod must block installation.");
                    Reject(() => manager.Apply(folder,true),"Unknown mod must block removal.");
                    Assert(File.ReadAllText(target) == "unrelated mod","Unknown mod must be preserved."); File.Delete(target);
                }
                passed.Add("PASS: unsupported engine and unknown files rejected before any change.");
                string core = Path.Combine(folder,"version.dll"), layer = Path.Combine(folder,"ufc6_menu_layer.dll");
                File.WriteAllBytes(core,Bytes("old core")); File.WriteAllBytes(layer,Bytes("old layer"));
                for (int fail = 0; fail < manager.Order.Count; ++fail) {
                    int point = fail;
                    manager.BeforeCommit = count => { if (count == point) throw new IOException("Injected commit failure"); };
                    Reject(() => manager.Apply(folder,false),"Injected failure must propagate.");
                    Assert(File.ReadAllText(core) == "old core" && File.ReadAllText(layer) == "old layer","Rollback must restore recognized old versions.");
                    Assert(!Directory.GetFiles(folder,"*.tmp").Any() && !Directory.GetFiles(folder,"*.bak").Any(),"Rollback must clean temporary files and restore backups.");
                }
                manager.BeforeCommit = null; manager.Apply(folder,false);
                Assert(Directory.GetFiles(folder,"*.bak").Length == 2,"Upgrade must keep original core and layer backups.");
                Assert(manager.Inspect(folder).Current == 4,"Upgrade must finish with all current binaries.");
                passed.Add("PASS: every install commit failure rolls back; recognized upgrades keep backups; core commits last.");
                for (int fail = 0; fail < manager.Order.Count; ++fail) {
                    int point = fail;
                    manager.BeforeCommit = count => { if (count == point) throw new IOException("Injected removal failure"); };
                    Reject(() => manager.Apply(folder,true),"Removal failure must propagate.");
                    Assert(manager.Inspect(folder).Current == 4,"Removal rollback must restore all installed files.");
                }
                manager.BeforeCommit = null; manager.Apply(folder,true);
                passed.Add("PASS: every removal commit failure restores all installed files.");
                manager.BeforeCommit = count => { if (count == 0) File.WriteAllBytes(layer,Bytes("concurrent mod")); };
                Reject(() => manager.Apply(folder,false),"Concurrent file change must block replacement.");
                Assert(File.ReadAllText(layer) == "concurrent mod" && !File.Exists(core),"Concurrent changes must not be overwritten.");
                manager.BeforeCommit = null; File.Delete(layer);
                Directory.CreateDirectory(layer); Reject(() => manager.Apply(folder,false),"Directory conflicts must be rejected."); Directory.Delete(layer);
                var bad = new Dictionary<string,byte[]>(payload); bad["version.dll"] = Bytes("corrupt payload");
                Reject(() => new Manager(release,bad),"Corrupt embedded payload must be rejected.");
                Assert(File.ReadAllText(ini) == settings && File.ReadAllText(screenshot) == "existing image","All safeguards must preserve user data.");
                passed.Add("PASS: concurrent writes, directory conflicts and corrupt bundled payloads are rejected.");
                // Verify the actual embedded release too, without touching a game directory.
                var embedded = new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<Release>(Encoding.UTF8.GetString(Program.Resource("release.json")).TrimStart('\ufeff'));
                new Manager(embedded,embedded.companion_files.Keys.Concat(new [] {"version.dll"}).ToDictionary(n => n,n => Program.Resource(n)));
                passed.Add("PASS: all actual bundled release files match the embedded manifest.");
                File.WriteAllLines(report,passed);
            } finally {
                // Only this randomly created test directory is removed.
                if (!Path.GetFullPath(root).StartsWith(Path.GetFullPath(Path.GetTempPath()),StringComparison.OrdinalIgnoreCase)) throw new IOException("Unsafe fixture path.");
                Directory.Delete(root,true);
            }
        }
    }
}

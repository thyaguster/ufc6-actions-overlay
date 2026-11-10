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
                var payload = new Dictionary<string,byte[]> { {"ufc6_menu_layer.dll",Bytes("new layer")}, {"ufc6_menu_layer.json",Bytes("new manifest")}, {"ufc6_menu_restart.exe",Bytes("new helper")}, {"ufc6_menu_update.exe",Bytes("update helper fixture")}, {"version.dll",Bytes("new core")} };
                var release = new Release { version = "fixture", engine_sha256 = Manager.HashFile(engine), dll_sha256 = Manager.Hash(payload["version.dll"]), previous_dll_sha256 = new [] {Manager.Hash(Bytes("old core"))}, companion_files = payload.Where(p => p.Key != "version.dll").ToDictionary(p => p.Key,p => Manager.Hash(p.Value)), previous_companion_sha256 = new Dictionary<string,string[]> { {"ufc6_menu_layer.dll",new [] {Manager.Hash(Bytes("old layer"))}} } };
                var manager = new Manager(release,payload);
                Assert(Manager.Normalize(Directory.GetParent(folder).FullName) == folder,"Game root must resolve to bin.");
                Assert(manager.Inspect(folder).Supported,"Fixture engine must be supported.");
                string parsec=Path.Combine(folder,"UFC 6 Parsec.exe");File.WriteAllBytes(parsec,Bytes("verified parsec fixture"));
                release.parsec_engine_sha256=Manager.HashFile(parsec);manager=new Manager(release,payload);
                Assert(manager.Inspect(folder).ParsecStatus.StartsWith("Verified"),"Parsec engine must be recognized in the same folder.");
                passed.Add("PASS: one shared install recognizes both normal and verified Parsec engines; no separate payload or steps.");

                string ini = Path.Combine(folder,"ufc6-quit-menu.ini"), screenshot = Path.Combine(folder,"existing-screenshot.png");
                File.WriteAllText(ini,"[Menu]\nUIScale=175\nAutoLaunchAfterRestart=0\nGuideButtonOpensMenu=0\n"); File.WriteAllBytes(screenshot,Bytes("existing image"));
                string settings = File.ReadAllText(ini);
                manager.Apply(folder,false); Assert(manager.Inspect(folder).Current == manager.Order.Count,"Installation must match every release hash.");
                manager.Apply(folder,false); Assert(!Directory.GetFiles(folder,"*.bak").Any(),"Idempotent install must not create backups.");
                manager.Apply(folder,true); Assert(manager.Inspect(folder).Installed == 0,"Remove must uninstall all managed files.");
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
                Assert(manager.Inspect(folder).Current == manager.Order.Count,"Upgrade must finish with all current binaries.");
                passed.Add("PASS: every install commit failure rolls back; recognized upgrades keep backups; core commits last.");
                for (int fail = 0; fail < manager.Order.Count; ++fail) {
                    int point = fail;
                    manager.BeforeCommit = count => { if (count == point) throw new IOException("Injected removal failure"); };
                    Reject(() => manager.Apply(folder,true),"Removal failure must propagate.");
                    Assert(manager.Inspect(folder).Current == manager.Order.Count,"Removal rollback must restore all installed files.");
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
                string runtime = Path.Combine(folder,"libwinpthread-1.dll"), originalCopy = Path.Combine(folder,"ufc6_pthread_original.dll");
                byte[] original = Bytes("user supplied original threading runtime");
                File.WriteAllBytes(runtime,original);
                payload["libwinpthread-1.dll"] = Bytes("startup bridge fixture");
                release.companion_files.Add("libwinpthread-1.dll",Manager.Hash(payload["libwinpthread-1.dll"]));
                release.runtime_bridge = new RuntimeBridge { target = "libwinpthread-1.dll", original_name = "ufc6_pthread_original.dll", original_sha256 = Manager.Hash(original) };
                var runtimeManager = new Manager(release,payload);
                for (int fail = 0; fail < runtimeManager.Order.Count; ++fail) {
                    int point = fail;
                    runtimeManager.BeforeCommit = count => { if (count == point) throw new IOException("Injected bridge install failure"); };
                    Reject(() => runtimeManager.Apply(folder,false),"Every bridge install failure must propagate.");
                    Assert(File.ReadAllBytes(runtime).SequenceEqual(original) && !File.Exists(originalCopy),"Failed bridge installation must preserve the original runtime.");
                    Assert(runtimeManager.Inspect(folder).Installed == 0,"Failed bridge installation must not leave a partial overlay.");
                }
                runtimeManager.BeforeCommit = null; runtimeManager.Apply(folder,false);
                Assert(File.ReadAllBytes(originalCopy).SequenceEqual(original),"Original runtime must be preserved byte-for-byte.");
                Assert(File.ReadAllBytes(runtime).SequenceEqual(payload["libwinpthread-1.dll"]),"Bridge must replace only the verified runtime.");
                for (int fail = 0; fail < runtimeManager.Order.Count; ++fail) {
                    int point = fail;
                    runtimeManager.BeforeCommit = count => { if (count == point) throw new IOException("Injected bridge removal failure"); };
                    Reject(() => runtimeManager.Apply(folder,true),"Bridge removal failure must propagate.");
                    Assert(runtimeManager.Inspect(folder).Current == runtimeManager.Order.Count,"Bridge removal rollback must restore all files, including the user's runtime copy.");
                }
                runtimeManager.BeforeCommit = null;
                File.Delete(originalCopy);
                Reject(() => runtimeManager.Apply(folder,true),"Missing original must block removal safely.");
                Reject(() => runtimeManager.Apply(folder,false),"Missing original must block reinstallation safely.");
                File.WriteAllBytes(originalCopy,Bytes("another mod"));
                Reject(() => runtimeManager.Apply(folder,true),"Unknown runtime copy must be preserved.");
                File.WriteAllBytes(originalCopy,original);
                runtimeManager.Apply(folder,true);
                Assert(File.ReadAllBytes(runtime).SequenceEqual(original) && !File.Exists(originalCopy),"Uninstall must restore the original runtime and remove the extra copy.");
                runtimeManager.Apply(folder,false); runtimeManager.Apply(folder,true);
                Assert(File.ReadAllBytes(runtime).SequenceEqual(original),"Reinstall after uninstall must remain safe.");
                Assert(File.ReadAllText(ini) == settings,"Runtime bridge migration must preserve settings.");
                passed.Add("PASS: runtime bridge install/remove/reinstall, every commit rollback, missing/corrupt original rejection, and byte-for-byte runtime restoration.");
                // A launcher rebuild has different bytes even when it uses our older sources.
                var bundled = new Dictionary<string,byte[]> {
                    {"version.dll",Bytes("PORTHEON rebuilt old core")},
                    {"libwinpthread-1.dll",Bytes("PORTHEON rebuilt old bridge")},
                    {"ufc6_menu_layer.dll",Bytes("PORTHEON rebuilt old layer")},
                    {"ufc6_menu_restart.exe",Bytes("PORTHEON rebuilt old restart helper")}
                };
                release.previous_dll_sha256 = release.previous_dll_sha256.Concat(new [] {Manager.Hash(bundled["version.dll"])}).ToArray();
                foreach(var item in bundled.Where(p => p.Key != "version.dll")) {
                    string[] old;release.previous_companion_sha256.TryGetValue(item.Key,out old);
                    release.previous_companion_sha256[item.Key]=(old??new string[0]).Concat(new [] {Manager.Hash(item.Value)}).ToArray();
                }
                runtimeManager=new Manager(release,payload);
                File.WriteAllBytes(originalCopy,original);
                foreach(var item in bundled)File.WriteAllBytes(Path.Combine(folder,item.Key),item.Value);
                File.WriteAllBytes(Path.Combine(folder,"ufc6_menu_layer.json"),payload["ufc6_menu_layer.json"]);
                Assert(runtimeManager.Inspect(folder).Conflicts.Count==0 && runtimeManager.Inspect(folder).RuntimeAvailable,"Verified launcher rebuild must be upgradable using the preserved original runtime.");
                var beforeMigration=runtimeManager.Order.ToDictionary(n=>n,n=>File.Exists(Path.Combine(folder,n))?Manager.HashFile(Path.Combine(folder,n)):null);
                for(int fail=0;fail<runtimeManager.Order.Count;++fail) {
                    int point=fail;runtimeManager.BeforeCommit=count=>{if(count==point)throw new IOException("Injected bundled migration failure");};
                    // Some files are already current: only actual commits can fail.
                    int changes=runtimeManager.Order.Count(n=>beforeMigration[n] != (n==release.runtime_bridge.original_name?release.runtime_bridge.original_sha256:n=="version.dll"?release.dll_sha256:release.companion_files[n]));
                    if(point>=changes)continue;
                    Reject(()=>runtimeManager.Apply(folder,false),"Bundled migration failure must propagate.");
                    Assert(beforeMigration.All(p=>(File.Exists(Path.Combine(folder,p.Key))?Manager.HashFile(Path.Combine(folder,p.Key)):null)==p.Value),"Bundled migration rollback must preserve every original file.");
                }
                runtimeManager.BeforeCommit=null;runtimeManager.Apply(folder,false);
                Assert(runtimeManager.Inspect(folder).Current==runtimeManager.Order.Count,"Bundled migration must install the complete current payload.");
                Assert(File.ReadAllText(ini)==settings,"Bundled migration must keep user preferences.");
                runtimeManager.Apply(folder,true);
                File.Delete(runtime);
                Assert(!runtimeManager.Inspect(folder).RuntimeAvailable,"Check folder must detect a missing original runtime before installation.");
                Assert(runtimeManager.Inspect(folder).Summary(runtimeManager).Contains("Original threading runtime is missing"),"Missing original diagnostic must be actionable.");
                File.WriteAllBytes(runtime,original);
                passed.Add("PASS: verified launcher rebuild migration, every migration rollback, complete startup bridge update, settings preservation and missing-runtime diagnostics.");
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

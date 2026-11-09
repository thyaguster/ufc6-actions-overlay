// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace UFC6OverlaySetup {
    public sealed class RuntimeBridge {
        public string target { get; set; }
        public string original_name { get; set; }
        public string original_sha256 { get; set; }
    }
    public sealed class Release {
        public string version { get; set; }
        public string engine_sha256 { get; set; }
        public string parsec_engine_sha256 { get; set; }
        public string dll_sha256 { get; set; }
        public string[] previous_dll_sha256 { get; set; }
        public Dictionary<string,string> companion_files { get; set; }
        public Dictionary<string,string[]> previous_companion_sha256 { get; set; }
        public RuntimeBridge runtime_bridge { get; set; }
    }
    public sealed class Inspection {
        public string Folder;
        public bool Supported;
        public string ParsecStatus;
        public readonly List<string> Conflicts = new List<string>();
        public readonly Dictionary<string,string> Hashes = new Dictionary<string,string>();
        public int Installed;
        public int Current;
        public string Summary(Manager manager) {
            var lines = new List<string>();
            lines.Add(Supported ? "Compatible PORTHEON 3309ab23f engine." : "Unsupported engine. Installation is disabled.");
            if (ParsecStatus != null) lines.Add(ParsecStatus);
            lines.Add(Installed == 0 ? "Overlay is not installed." : Current == manager.Order.Count ? "Overlay v" + manager.Version + " is already installed." : "An existing overlay installation was found; it can be updated or removed.");
            if (Conflicts.Count != 0) lines.Add("Conflicting or linked files: " + string.Join(", ", Conflicts) + ". These files will not be replaced or removed.");
            lines.Add("\r\nSettings and screenshots are kept. Close UFC 6 before making changes.");
            return string.Join("\r\n", lines);
        }
    }
    public sealed class Manager {
        public readonly List<string> Order = new List<string>();
        public readonly Dictionary<string,byte[]> Payload;
        readonly Dictionary<string,string> expected = new Dictionary<string,string>();
        readonly Dictionary<string,HashSet<string>> known = new Dictionary<string,HashSet<string>>();
        readonly string engineHash;
        readonly string parsecHash;
        readonly RuntimeBridge runtime;
        public readonly string Version;
        // Tests inject fixture bytes and a fixture engine hash into the same transaction code.
        public Action<int> BeforeCommit;
        public Manager(Release release, Dictionary<string,byte[]> payload) {
            Version = release.version; engineHash = release.engine_sha256; parsecHash = release.parsec_engine_sha256; Payload = payload; runtime = release.runtime_bridge;
            foreach (var pair in release.companion_files) { expected.Add(pair.Key,pair.Value); Order.Add(pair.Key); }
            expected.Add("version.dll",release.dll_sha256); Order.Add("version.dll"); // Core commits last.
            if (runtime != null) {
                if (!expected.ContainsKey(runtime.target) || runtime.target == "version.dll") throw new InvalidDataException("Invalid runtime bridge target.");
                expected.Add(runtime.original_name,runtime.original_sha256); Order.Insert(0,runtime.original_name);
            }
            foreach (string name in Order) {
                if (Path.GetFileName(name) != name || name.IndexOfAny(new [] {'/', '\\', ':'}) >= 0) throw new InvalidDataException("Invalid release filename.");
                string[] old;
                if (name == "version.dll") old = release.previous_dll_sha256;
                else if (!release.previous_companion_sha256.TryGetValue(name,out old)) old = new string[0];
                known[name] = new HashSet<string>(old ?? new string[0],StringComparer.OrdinalIgnoreCase);
                known[name].Add(expected[name]);
                if (runtime != null && name == runtime.target) known[name].Add(runtime.original_sha256);
                if (runtime != null && name == runtime.original_name) continue; // User's original; never bundled.
                if (!payload.ContainsKey(name) || Hash(payload[name]) != expected[name]) throw new InvalidDataException("The bundled " + name + " failed verification.");
            }
        }
        public static string Hash(byte[] bytes) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant(); }
        public static string HashFile(string path) { using (var sha = SHA256.Create()) using (var file = File.OpenRead(path)) return BitConverter.ToString(sha.ComputeHash(file)).Replace("-","").ToLowerInvariant(); }
        static bool Linked(string path) { return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0; }
        public static string Normalize(string folder) {
            folder = Path.GetFullPath(folder.Trim().Trim('"'));
            if (!Directory.Exists(folder)) throw new IOException("Choose the UFC 6 folder or its bin folder.");
            if (!File.Exists(Path.Combine(folder,"UFC 6.exe")) && File.Exists(Path.Combine(folder,"bin","UFC 6.exe"))) folder = Path.Combine(folder,"bin");
            if (Linked(folder)) throw new IOException("Choose a real game folder, rather than a linked folder.");
            if (!File.Exists(Path.Combine(folder,"UFC 6.exe"))) throw new IOException("UFC 6.exe was not found. Choose the game's bin folder, not the PORTHEON program folder.");
            return folder;
        }
        public Inspection Inspect(string folder) {
            folder = Normalize(folder);
            if (Linked(Path.Combine(folder,"UFC 6.exe"))) throw new IOException("The game executable is a link. Nothing was changed.");
            var result = new Inspection { Folder = folder, Supported = HashFile(Path.Combine(folder,"UFC 6.exe")) == engineHash };
            string parsec = Path.Combine(folder,"UFC 6 Parsec.exe");
            if (parsecHash != null && File.Exists(parsec)) result.ParsecStatus = !Linked(parsec) && HashFile(parsec) == parsecHash ? "Verified Parsec / two-player engine is supported." : "Unrecognized Parsec engine: that mode will not load the overlay.";
            foreach (string name in Order) {
                string path = Path.Combine(folder,name);
                if (Directory.Exists(path) || (File.Exists(path) && Linked(path))) { result.Conflicts.Add(name); continue; }
                string hash = File.Exists(path) ? HashFile(path) : null;
                result.Hashes[name] = hash;
                if (hash == null) continue;
                if (!known[name].Contains(hash)) result.Conflicts.Add(name);
                else {
                    bool original = runtime != null && (name == runtime.original_name || (name == runtime.target && hash == runtime.original_sha256));
                    if (!original) ++result.Installed;
                    if (hash == expected[name]) ++result.Current;
                }
            }
            return result;
        }
        public static void RequireClosed(string folder) {
            // Do not terminate anything. A matching process that cannot be inspected is also left alone.
            foreach (var process in Process.GetProcesses()) using (process) {
                string name;
                try { name = process.ProcessName; } catch { continue; }
                if (!string.Equals(name,"UFC 6",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"UFC 6 Parsec",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"UFC6",StringComparison.OrdinalIgnoreCase) && !string.Equals(name,"ufc6_menu_restart",StringComparison.OrdinalIgnoreCase)) continue;
                try {
                    string path = process.MainModule.FileName;
                    if (string.Equals(Path.GetDirectoryName(path),folder,StringComparison.OrdinalIgnoreCase) || string.Equals(path,Path.Combine(Directory.GetParent(folder).FullName,"UFC6.exe"),StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Close UFC 6 and its launcher before installing, updating, or removing the overlay.");
                } catch (Win32Exception) { throw new InvalidOperationException("A UFC 6 process is running. Close the game and launcher, then try again."); }
                catch (InvalidOperationException) { throw; }
            }
        }
        static void Unchanged(string path,string hash) {
            if (Directory.Exists(path) || (File.Exists(path) && Linked(path)) || (File.Exists(path) ? HashFile(path) : null) != hash) throw new IOException("A file changed during setup. No replacement was attempted: " + Path.GetFileName(path));
        }
        public string Apply(string folder,bool remove) {
            var state = Inspect(folder); folder = state.Folder;
            if (state.Conflicts.Count != 0) throw new IOException("Unknown or linked files were found: " + string.Join(", ",state.Conflicts) + ". Nothing was changed.");
            if (!remove && !state.Supported) throw new IOException("This engine is not the supported PORTHEON 3309ab23f build. Nothing was changed.");
            RequireClosed(folder);
            byte[] originalRuntime = null;
            if (runtime != null) {
                string source = state.Hashes[runtime.target] == runtime.original_sha256 ? runtime.target : state.Hashes[runtime.original_name] == runtime.original_sha256 ? runtime.original_name : null;
                if (source == null) throw new IOException("The verified original threading runtime is missing. Nothing was changed. Restore it from your own matching PORTHEON installation.");
                originalRuntime = File.ReadAllBytes(Path.Combine(folder,source));
                if (Hash(originalRuntime) != runtime.original_sha256) throw new IOException("The original runtime changed during setup. Nothing was changed.");
                Payload[runtime.original_name] = originalRuntime;
            }
            var desired = Order.ToDictionary(name => name,name => !remove ? expected[name] : runtime != null && name == runtime.target ? runtime.original_sha256 : null);
            var order = remove ? Order.AsEnumerable().Reverse().ToList() : Order;
            var staged = new Dictionary<string,string>();
            var backups = new Dictionary<string,string>();
            var committed = new List<string>();
            var installed = new HashSet<string>();
            string token = Guid.NewGuid().ToString("N");
            try {
                foreach (string name in order) {
                    string old = state.Hashes[name];
                    if (old == desired[name]) continue;
                    if (desired[name] != null) {
                        string temp = Path.Combine(folder,".ufc6-menu-" + token + "-" + name + ".tmp");
                        using (var file = new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)) { staged.Add(name,temp); byte[] bytes = remove ? originalRuntime : Payload[name]; file.Write(bytes,0,bytes.Length); file.Flush(true); }
                        if (HashFile(temp) != desired[name]) throw new IOException("Temporary payload verification failed.");
                    }
                    if (old != null) backups[name] = Path.Combine(folder,"ufc6-quit-menu." + (remove ? "removed-" : "previous-") + name + "-" + old.Substring(0,16) + "-" + token + ".bak");
                }
                foreach (string name in order) {
                    if (!staged.ContainsKey(name) && !backups.ContainsKey(name)) continue;
                    if (BeforeCommit != null) BeforeCommit(committed.Count);
                    string target = Path.Combine(folder,name);
                    Unchanged(target,state.Hashes[name]);
                    committed.Add(name);
                    if (backups.ContainsKey(name)) File.Move(target,backups[name]);
                    if (staged.ContainsKey(name)) { File.Move(staged[name],target); installed.Add(name); if (HashFile(target) != desired[name]) throw new IOException("Installed file verification failed."); }
                }
            } catch (Exception error) {
                var failures = new List<string>();
                foreach (string name in committed.AsEnumerable().Reverse()) {
                    string target = Path.Combine(folder,name);
                    try {
                        if (installed.Contains(name) && File.Exists(target) && !Linked(target) && HashFile(target) == desired[name]) File.Delete(target);
                        if (backups.ContainsKey(name) && File.Exists(backups[name]) && !File.Exists(target) && !Directory.Exists(target)) File.Move(backups[name],target);
                        if ((File.Exists(target) ? HashFile(target) : null) != state.Hashes[name]) failures.Add(name);
                    } catch { failures.Add(name); }
                }
                if (failures.Count != 0) throw new IOException(error.Message + "\r\nAutomatic rollback needs attention for: " + string.Join(", ",failures) + ". Original files remain in the adjacent backup files.",error);
                throw;
            } finally { foreach (string path in staged.Values) { try { if (File.Exists(path)) File.Delete(path); } catch { } } }
            if (remove) foreach (string path in backups.Values) { try { File.Delete(path); } catch { /* The mod is removed; leave any locked backup in place. */ } }
            return remove ? "Overlay removed. Settings, screenshots, and upgrade backups were kept." : "Overlay v" + Version + " installed.\r\nLaunch UFC 6 normally, then hold LB + RB + Menu for about one second, or press F10.\r\n\r\nSettings and screenshots were kept. Recognized older files were backed up.";
        }
    }
    public sealed class SetupForm : Form {
        readonly Manager manager;
        readonly TextBox folder = new TextBox();
        readonly TextBox status = new TextBox();
        readonly Button browse = new Button(), check = new Button(), install = new Button(), remove = new Button();
        bool busy;
        public SetupForm(Manager manager) {
            this.manager = manager;
            Text = "UFC 6 Actions Overlay Setup — v" + manager.Version;
            Font = new Font("Segoe UI",10); ClientSize = new Size(680,440); MinimumSize = Size; MaximumSize = Size;
            FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = false; StartPosition = FormStartPosition.CenterScreen; AutoScaleMode = AutoScaleMode.Dpi;
            var title = new Label { Text = "UFC 6 Actions Overlay", Font = new Font("Segoe UI",18,FontStyle.Bold), Bounds = new Rectangle(24,20,630,42) };
            var hint = new Label { Text = "Choose the UFC 6 game folder or its bin folder.\r\nSupports the PORTHEON 3309ab23f Windows engine.", Bounds = new Rectangle(26,68,620,52) };
            folder.SetBounds(26,129,500,28); browse.Text = "Browse…"; browse.SetBounds(538,127,114,32);
            check.Text = "Check folder"; check.SetBounds(26,170,140,34);
            status.SetBounds(26,215,626,142); status.Multiline = true; status.ReadOnly = true; status.BorderStyle = BorderStyle.FixedSingle; status.BackColor = SystemColors.Window; status.ScrollBars = ScrollBars.Vertical;
            status.Text = "Select your game folder, then click Check folder.\r\nNo files are changed until you choose Install / Update or Remove.";
            install.Text = "Install / Update"; install.SetBounds(306,378,170,38); install.Enabled = false;
            remove.Text = "Remove overlay"; remove.SetBounds(488,378,164,38); remove.Enabled = false;
            Controls.AddRange(new Control[] {title,hint,folder,browse,check,status,install,remove});
            AcceptButton = check;
            browse.Click += delegate { using (var dialog = new FolderBrowserDialog { Description = "Select the UFC 6 game folder or its bin folder", ShowNewFolderButton = false }) { if (Directory.Exists(folder.Text)) dialog.SelectedPath = folder.Text; if (dialog.ShowDialog(this) == DialogResult.OK) { folder.Text = dialog.SelectedPath; Run(null); } } };
            folder.TextChanged += delegate { install.Enabled = remove.Enabled = false; };
            check.Click += delegate { Run(null); };
            install.Click += delegate { Run(false); };
            remove.Click += delegate { if (MessageBox.Show(this,"Remove the overlay from this game folder? Settings and screenshots will be kept.","Remove overlay",MessageBoxButtons.YesNo,MessageBoxIcon.Question,MessageBoxDefaultButton.Button2) == DialogResult.Yes) Run(true); };
            FormClosing += delegate(object sender,FormClosingEventArgs e) { if (busy) e.Cancel = true; };
        }
        void Run(bool? removal) {
            if (busy) return;
            busy = true; string path = folder.Text;
            folder.Enabled = browse.Enabled = check.Enabled = install.Enabled = remove.Enabled = false;
            status.Text = removal.HasValue ? "Applying changes…" : "Checking engine and installed files…"; UseWaitCursor = true;
            var worker = new BackgroundWorker();
            worker.DoWork += delegate(object sender,DoWorkEventArgs e) {
                string message = removal.HasValue ? manager.Apply(path,removal.Value) : null;
                Inspection state = manager.Inspect(path);
                e.Result = new object[] { state,message };
            };
            worker.RunWorkerCompleted += delegate(object sender,RunWorkerCompletedEventArgs e) {
                busy = false; UseWaitCursor = false; folder.Enabled = browse.Enabled = check.Enabled = true;
                if (e.Error != null) { status.Text = e.Error.Message + "\r\n\r\nIf Windows denies access, close the game first. For a protected game folder, right-click this setup and choose Run as administrator."; }
                else {
                    var values = (object[])e.Result; var state = (Inspection)values[0]; folder.Text = state.Folder;
                    status.Text = values[1] == null ? state.Summary(manager) : (string)values[1];
                    install.Enabled = state.Supported && state.Conflicts.Count == 0; remove.Enabled = state.Installed > 0 && state.Conflicts.Count == 0;
                }
                worker.Dispose();
            };
            worker.RunWorkerAsync();
        }
    }
    public static class Program {
        public static byte[] Resource(string name) { using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) { if (stream == null) throw new IOException("Missing bundled resource: " + name); using (var memory = new MemoryStream()) { stream.CopyTo(memory); return memory.ToArray(); } } }
        static Manager Bundled() {
            var release = new JavaScriptSerializer().Deserialize<Release>(Encoding.UTF8.GetString(Resource("release.json")).TrimStart('\ufeff'));
            var files = release.companion_files.Keys.Concat(new [] {"version.dll"}).ToDictionary(name => name,name => Resource(name));
            return new Manager(release,files);
        }
        [STAThread] public static int Main(string[] args) {
            try {
                if (args.Length == 2 && args[0] == "--self-test") { SelfTests.Run(args[1]); return 0; }
                Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
                var manager = Bundled();
                using (var form = new SetupForm(manager)) {
                    if (args.Length == 2 && args[0] == "--preview") { form.Show(); Application.DoEvents(); using (var bitmap = new Bitmap(form.Width,form.Height)) { form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size)); bitmap.Save(args[1]); } return 0; }
                    Application.Run(form);
                }
                return 0;
            } catch (Exception error) {
                if (args.Length == 2 && args[0] == "--self-test") { File.WriteAllText(args[1],"FAIL: " + error); return 1; }
                MessageBox.Show(error.Message,"UFC 6 Actions Overlay Setup",MessageBoxButtons.OK,MessageBoxIcon.Error); return 1;
            }
        }
    }
}

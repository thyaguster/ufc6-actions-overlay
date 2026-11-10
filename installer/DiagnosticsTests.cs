// SPDX-License-Identifier: GPL-2.0-only
using System;using System.IO;using System.IO.Compression;using System.Linq;using System.Text;
namespace UFC6OverlaySetup {
 public static class DiagnosticsTests {
  static void Require(bool condition,string message){if(!condition)throw new Exception(message);}
  public static string Run(){
   string root=Path.Combine(Path.GetTempPath(),"UFC6 support tests "+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
   try{
    string game=Path.Combine(root,"game"),bin=Path.Combine(game,"bin"),destination=Path.Combine(root,"reports");Directory.CreateDirectory(bin);
    File.WriteAllText(Path.Combine(bin,"UFC 6.exe"),"fixture engine");File.WriteAllText(Path.Combine(bin,"version.dll"),"fixture core");
    File.WriteAllText(Path.Combine(bin,"ufc6-quit-menu.log"),"UFC6 quit menu loaded inside engine process.\nController hooks installed.\nPrivate path C:\\Users\\PrivateName\\Game\\bin\n");
    Directory.CreateDirectory(Path.Combine(bin,"_SaveData"));File.WriteAllText(Path.Combine(bin,"_SaveData","secret-save"),"must never be included");
    File.WriteAllText(Path.Combine(bin,"ufc6-quit-menu.ini"),"LaunchArgs=private-launch-argument");
    string one=Diagnostics.Export(game,destination,"test","inactive",123),two=Diagnostics.Export(bin,destination,"test");
    Require(one!=two&&File.Exists(one)&&File.Exists(two),"Repeated reports must keep older archives.");
    using(var zip=ZipFile.OpenRead(one)){
     Require(zip.Entries.Count==4,"Report must contain only its manifest, README and two allowlisted logs.");
     string all="";foreach(var e in zip.Entries)using(var reader=new StreamReader(e.Open()))all+=reader.ReadToEnd();
     Require(!all.Contains("PrivateName")&&!all.Contains("secret-save")&&!all.Contains("private-launch-argument"),"Report must exclude private paths, save contents and launch arguments.");
     Require(all.Contains("inactive")&&all.Contains("controllerHooks")&&all.Contains("Log is missing.")&&all.Contains("sha256"),"Report must explain runtime, hashes and missing log state.");
     Require(!all.Contains(root),"Report must omit absolute game paths.");
    }
    File.WriteAllText(Path.Combine(bin,"ufc6-quit-menu-vulkan.log"),new string('x',600*1024)+"\nMenu drawn directly into a presented game frame.\n");
    string bounded=Diagnostics.Export(bin,destination,"test","active");
    using(var zip=ZipFile.OpenRead(bounded))Require(zip.GetEntry("logs/ufc6-quit-menu-vulkan.log").Length<513*1024,"Diagnostic logs must be bounded.");
    bool rejected=false;try{Diagnostics.Export(bin,Path.Combine(bin,"_SaveData","reports"),"test");}catch(IOException){rejected=true;}
    Require(rejected&&!Directory.Exists(Path.Combine(bin,"_SaveData","reports")),"Reports must not be written inside game or save folders.");
    Require(!Directory.GetFiles(destination,"*.partial").Any(),"Completed reports must leave no partial file.");
    Require(Diagnostics.Redact(@"failure at \\PrivateServer\PrivateShare\file").Contains("[network path]"),"Network paths must be redacted.");
    return "PASS: diagnostics allowlist, path redaction, missing logs/components, bounded logs, distinct verified archives, save/argument exclusion and game-folder write rejection.";
   }finally{if(!Path.GetFullPath(root).StartsWith(Path.GetFullPath(Path.GetTempPath()),StringComparison.OrdinalIgnoreCase))throw new IOException("Unsafe fixture path.");Directory.Delete(root,true);}
  }
 }
}

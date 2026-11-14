// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
namespace UFC6OverlaySetup {
 public static class Diagnostics {
  public static readonly string[] Files={"UFC 6.exe","UFC 6 Parsec.exe","UFC 6 novo.exe","version.dll","libwinpthread-1.dll","winmm.dll","ufc6_pthread_original.dll","ufc6_menu_layer.dll","ufc6_menu_layer.json","ufc6_menu_restart.exe","ufc6_menu_icons.exe","ufc6_menu_update.exe"};
  static readonly string[] Logs={"ufc6-quit-menu.log","ufc6-quit-menu-vulkan.log"};
  const int MaxLog=512*1024;
  static Dictionary<string,string> Expected(){using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("diagnostics-components.json")){if(stream==null)return new Dictionary<string,string>();using(var reader=new StreamReader(stream))return new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(reader.ReadToEnd().TrimStart('\ufeff'));}}

  static bool Linked(string path){return (File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0;}
  public static string Folder(string path){
   path=Path.GetFullPath(path);
   if(!File.Exists(Path.Combine(path,"UFC 6.exe"))&&File.Exists(Path.Combine(path,"bin","UFC 6.exe")))path=Path.Combine(path,"bin");
   if(!Directory.Exists(path)||Linked(path)||(!File.Exists(Path.Combine(path,"UFC 6.exe"))&&!File.Exists(Path.Combine(path,"UFC 6 novo.exe"))))throw new IOException("Choose PORTHEON's UFC6, bin, or novo game folder.");
   return path;
  }
  public static string Redact(string value){
   value=Regex.Replace(value,@"(?i)\b[A-Z]:[\\/][^\r\n""<>|]*","[path]");
   value=Regex.Replace(value,@"\\\\[^\r\n""<>|]+","[network path]");
   return value;
  }
  static Dictionary<string,object> FileInfo(string folder,string name){
   var row=new Dictionary<string,object>{{"name",name}};string expected;Expected().TryGetValue(name,out expected);if(expected!=null)row["expectedSha256"]=expected;string path=Path.Combine(folder,name);
   try{
    if(Directory.Exists(path)){row["status"]="directory-conflict";return row;}
    if(!File.Exists(path)){row["status"]="missing";return row;}
    if(Linked(path)){row["status"]="linked-file-skipped";return row;}
    using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
     row["bytes"]=file.Length;
     if(file.Length>64*1024*1024){row["status"]="too-large-skipped";return row;}
     using(var sha=SHA256.Create())row["sha256"]=BitConverter.ToString(sha.ComputeHash(file)).Replace("-","").ToLowerInvariant();
    }
    row["status"]="present";if(expected!=null)row["matchesReporterBuild"]=string.Equals((string)row["sha256"],expected,StringComparison.OrdinalIgnoreCase);
   }catch(Exception e){row["status"]="unavailable: "+e.GetType().Name;}
   return row;
  }
  static string Tail(string folder,string name){
   string path=Path.Combine(folder,name);
   if(!File.Exists(path))return "Log is missing.\n";
   if(Linked(path))return "Linked log was skipped.\n";
   try{
    using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
     long length=file.Length;bool truncated=length>MaxLog;file.Position=Math.Max(0,length-MaxLog);
     byte[] data=new byte[(int)Math.Min(length,MaxLog)];int total=0,n;
     while(total<data.Length&&(n=file.Read(data,total,data.Length-total))>0)total+=n;
     string text=Encoding.UTF8.GetString(data,0,total);
     if(truncated){int newline=text.IndexOf('\n');if(newline>=0)text=text.Substring(newline+1);text="[Earlier log lines omitted; last 512 KiB only.]\n"+text;}
     return Redact(text);
    }
   }catch(Exception e){return "Log unavailable: "+e.GetType().Name+"\n";}
  }
  static List<object> Processes(string folder,int requestedPid){
   var found=new List<object>();
   foreach(string name in new[]{"UFC 6","UFC 6 Parsec","UFC 6 novo"})foreach(var process in Process.GetProcessesByName(name))using(process){
    try{
     if(!string.Equals(Path.GetDirectoryName(process.MainModule.FileName),folder,StringComparison.OrdinalIgnoreCase))continue;
     var modules=new List<string>();
     try{foreach(ProcessModule module in process.Modules){string leaf=Path.GetFileName(module.FileName);if(Files.Contains(leaf,StringComparer.OrdinalIgnoreCase)||string.Equals(leaf,"vulkan-1.dll",StringComparison.OrdinalIgnoreCase))modules.Add(leaf);}}catch{modules.Add("Module inspection unavailable.");}
     found.Add(new{engine=name+".exe",pid=process.Id,requested=process.Id==requestedPid,modules=modules});
    }catch{ /* Do not report other users' or unrelated process details. */ }
   }
   return found;
  }
  static object LoaderMessages(string folder){
   var messages=new List<object>();string logs=Path.Combine(folder,"_Logs");
   try{
    if(!Directory.Exists(logs)||Linked(logs))return new{available=false,messages=messages};
    foreach(string path in Directory.GetFiles(logs,"*.log").OrderByDescending(File.GetLastWriteTimeUtc).Take(2)){
     if(Linked(path))continue;
     using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
      const int limit=128*1024;byte[] head=new byte[(int)Math.Min(file.Length,limit)];int count=file.Read(head,0,head.Length);string text=Encoding.UTF8.GetString(head,0,count);
      if(file.Length>limit){file.Position=Math.Max(limit,file.Length-limit);byte[] tail=new byte[(int)(file.Length-file.Position)];count=file.Read(tail,0,tail.Length);text+="\n"+Encoding.UTF8.GetString(tail,0,count);}
      int added=0;foreach(string line in text.Split('\n')){
       if(!Regex.IsMatch(line,@"(?i)(vulkan.?loader|\[loader\].*(error|warn)|failed to (find|load).*layer|vk_layer_ufc6)"))continue;
       messages.Add(new{log=Path.GetFileName(path),text=Redact(line.Length>1024?line.Substring(0,1024):line)});if(++added>=32)break;
      }
     }
    }
    return new{available=true,messages=messages};
   }catch{return new{available=false,messages=messages};}
  }
  public static string Export(string folder,string destination,string version,string renderer="unknown",int pid=0){
   folder=Folder(folder);destination=Path.GetFullPath(destination);
   string gameRoot=Directory.GetParent(folder).FullName.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
   if(destination.Equals(folder,StringComparison.OrdinalIgnoreCase)||destination.StartsWith(gameRoot,StringComparison.OrdinalIgnoreCase))throw new IOException("Save support reports outside the game folder.");
   if(renderer!="active"&&renderer!="inactive"&&renderer!="unknown")throw new ArgumentException("Invalid renderer state.");
   Directory.CreateDirectory(destination);
   if(Linked(destination))throw new IOException("Choose a real support-report folder.");
   var files=Files.Select(n=>FileInfo(folder,n)).ToList();
   var logs=Logs.ToDictionary(n=>n,n=>Tail(folder,n));
   string coreLog=logs[Logs[0]],vulkanLog=logs[Logs[1]];
   var report=new {
    format=1,createdUtc=DateTime.UtcNow.ToString("o"),reporterVersion=version,windows=Environment.OSVersion.VersionString,is64BitOS=Environment.Is64BitOperatingSystem,
    runtimeSnapshot=new{rendererAtExport=renderer,requestedProcessId=pid},
    files=files,processes=Processes(folder,pid),vulkanLoaderEvidence=LoaderMessages(folder),
    logEvidence=new{note="Logs can contain previous sessions; runtime snapshot is from the export request.",coreLoaded=coreLog.Contains("loaded inside engine process"),controllerHooks=coreLog.Contains("Controller hooks installed"),layerAttached=vulkanLog.Contains("Vulkan menu layer attached"),swapchainReady=vulkanLog.Contains("Swapchain menu resources ready"),frameDrawn=vulkanLog.Contains("Menu drawn directly into")},
    privacy="Only component fingerprints, selected runtime/module information two bounded overlay logs and filtered Vulkan-loader messages. Paths are redacted. No saves, screenshots, launcher arguments, account names or game content are collected."
   };
   string token=DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8);
   string output=Path.Combine(destination,"UFC6-support-"+token+".zip"),temp=output+".partial";
   try{
    using(var file=new FileStream(temp,FileMode.CreateNew,FileAccess.ReadWrite,FileShare.None)){
     using(var archive=new ZipArchive(file,ZipArchiveMode.Create,true)){
      Write(archive,"report.json",new JavaScriptSerializer().Serialize(report));
      Write(archive,"README.txt","UFC 6 Actions Overlay support report\r\nReview this ZIP before sharing it. No files were uploaded.\r\nLog evidence may include old sessions; compare timestamps/build lines with the runtime snapshot.\r\n");
      foreach(var log in logs)Write(archive,"logs/"+log.Key,log.Value);
     }
     file.Flush(true);
    }
    using(var archive=ZipFile.OpenRead(temp)){
     if(archive.Entries.Count!=4)throw new IOException("Support report verification failed.");
     foreach(var entry in archive.Entries)using(var stream=entry.Open()){byte[] buffer=new byte[8192];while(stream.Read(buffer,0,buffer.Length)>0){} }
    }
    File.Move(temp,output);return output;
   }finally{if(File.Exists(temp))File.Delete(temp);}
  }
  static void Write(ZipArchive archive,string name,string value){using(var stream=archive.CreateEntry(name,CompressionLevel.Optimal).Open())using(var writer=new StreamWriter(stream,new UTF8Encoding(false)))writer.Write(value);}
 }
}

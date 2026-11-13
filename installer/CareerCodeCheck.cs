// SPDX-License-Identifier: GPL-2.0-only
// Standalone read-only inspection. No injection, hooks, protection changes or game writes.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

static class CareerCodeCheck {
 const ulong Guest=0x900000000;
 const uint ReadQuery=0x410;
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] data,UIntPtr count,out UIntPtr read);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 sealed class Site {
  public readonly string Name;public readonly ulong Offset;public readonly byte[] Expected;
  public Site(string name,ulong offset,string hex){Name=name;Offset=offset;Expected=Hex(hex);}
 }
 static readonly Site[] Sites={
  new Site("Selected-user lookup",0x5c015d0,"55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 ec 28 48 8b 05 c8 6c 13 04"),
  new Site("Career field getter",0x56dcf80,"48 8b 47 08 89 d1 4c 8d 04 89 49 c1 e0 04"),
  new Site("Career field setter",0x56dd300,"48 8b 47 08 41 89 d0 48 8b 7f 30 4f 8d 04 80"),
  new Site("Career dirty marker",0x56ccef0,"48 8b 47 10 ff 80 e8 00 00 00 c3")
 };
 static byte[] Hex(string text){return text.Split(' ').Select(x=>Convert.ToByte(x,16)).ToArray();}
 static string Hex(byte[] bytes){return BitConverter.ToString(bytes).Replace('-',' ');}
 static string Hash(string path){using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var hash=SHA256.Create())return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-","").ToLowerInvariant();}
 static byte[] Read(IntPtr process,ulong address,int count){
  if(address<0x10000||address>0x7fffffffffffUL-(uint)count||count<1||count>64)return new byte[0];
  byte[] data=new byte[count];UIntPtr read;
  return ReadProcessMemory(process,new IntPtr((long)address),data,(UIntPtr)(uint)count,out read)&&read.ToUInt64()==(ulong)count?data:new byte[0];
 }
 static bool Matches(byte[] actual,byte[] expected){return actual.Length>=expected.Length&&actual.Take(expected.Length).SequenceEqual(expected);}
 static ulong Jump(IntPtr process,ulong address,byte[] bytes){
  if(bytes.Length>=5&&bytes[0]==0xe9)return unchecked((ulong)((long)address+5+BitConverter.ToInt32(bytes,1)));
  if(bytes.Length>=6&&bytes[0]==0xff&&bytes[1]==0x25){
   ulong slot=unchecked((ulong)((long)address+6+BitConverter.ToInt32(bytes,2)));byte[] value=Read(process,slot,8);return value.Length==8?BitConverter.ToUInt64(value,0):0;
  }
  if(bytes.Length>=12&&bytes[0]==0x48&&bytes[1]==0xb8&&bytes[10]==0xff&&bytes[11]==0xe0)return BitConverter.ToUInt64(bytes,2);
  return 0;
 }
 static string Owner(Process process,ulong address){
  try {foreach(ProcessModule module in process.Modules){ulong start=(ulong)module.BaseAddress.ToInt64();if(address>=start&&address-start<(ulong)module.ModuleMemorySize)return Path.GetFileName(module.FileName);}}
  catch {return "module ownership unavailable";}
  return "outside listed modules (a private relay is possible)";
 }
 static bool Supported(string hash){return hash=="f3c42126bd9df5c12cb69c1c57233a94ba024c50929ba0dfe20e715521f691a2"||hash=="ad94dc7e30ad12c8078d14d96520c1d48d4b0c658604cc3dcc1d68a3e228773d";}
 static string Inspect(){
  var output=new StringBuilder();output.AppendLine("UFC 6 career-code check v1");output.AppendLine("UTC: "+DateTime.UtcNow.ToString("o"));
  output.AppendLine("Read-only: four career function headers, one registration name and loaded overlay module fingerprints. No saves or game assets are read. Nothing is uploaded.");
  var games=Process.GetProcesses().Where(p=>p.ProcessName.Equals("UFC 6",StringComparison.OrdinalIgnoreCase)||p.ProcessName.Equals("UFC 6 Parsec",StringComparison.OrdinalIgnoreCase)).ToArray();
  if(games.Length!=1){output.AppendLine("Expected exactly one running UFC 6 engine; found "+games.Length+". Leave the saved Legacy career open and retry.");foreach(var item in games)item.Dispose();return output.ToString();}
  using(var game=games[0]){
   string path=game.MainModule.FileName,hash=Hash(path);output.AppendLine("Engine: "+Path.GetFileName(path));output.AppendLine("Engine SHA256: "+hash);
   if(!Supported(hash)){output.AppendLine("Unsupported engine identity. No process memory inspected.");return output.ToString();}
   IntPtr handle=OpenProcess(ReadQuery,false,game.Id);
   if(handle==IntPtr.Zero){output.AppendLine("Cannot open process read-only. Windows error "+Marshal.GetLastWin32Error()+". Run the game and checker at the same normal privilege level.");return output.ToString();}
   try {
    foreach(var site in Sites){
     byte[] actual=Read(handle,Guest+site.Offset,32);output.AppendLine();output.AppendLine(site.Name+" at guest +0x"+site.Offset.ToString("x"));
     output.AppendLine("Expected: "+Hex(site.Expected));output.AppendLine("Actual: "+(actual.Length==0?"UNREADABLE":Hex(actual)));
     output.AppendLine("Result: "+(actual.Length==0?"unreadable":Matches(actual,site.Expected)?"exact match":"mismatch"));
     ulong target=Jump(handle,Guest+site.Offset,actual);
     if(target!=0){output.AppendLine("Jump target: 0x"+target.ToString("x")+"; "+Owner(game,target));output.AppendLine("A jump can be an existing overlay hook; this alone does not identify a conflicting mod.");}
    }
    byte[] slot=Read(handle,Guest+0xb452958,8);ulong flag=slot.Length==8?BitConverter.ToUInt64(slot,0):0;
    byte[] nameSlot=flag!=0?Read(handle,flag+8,8):new byte[0];ulong name=nameSlot.Length==8?BitConverter.ToUInt64(nameSlot,0):0;
    byte[] text=name!=0?Read(handle,name,28):new byte[0];
    output.AppendLine();output.AppendLine("Old stamina-registration gate: "+(Matches(text,Encoding.ASCII.GetBytes("Fighter_Disable_StaminaLoss").Take(26).ToArray())?"present":"absent or different"));
    output.AppendLine();output.AppendLine("Loaded overlay modules:");
    try {foreach(ProcessModule module in game.Modules)if(new[]{"version.dll","ufc6_menu_layer.dll","libwinpthread-1.dll","ufc6_pthread_original.dll"}.Contains(Path.GetFileName(module.FileName),StringComparer.OrdinalIgnoreCase))output.AppendLine(Path.GetFileName(module.FileName)+" SHA256="+Hash(module.FileName));}
    catch {output.AppendLine("Some module fingerprints were unavailable.");}
   }finally{CloseHandle(handle);}
  }
  return output.ToString();
 }
 static void Require(bool value){if(!value)throw new InvalidOperationException("Self-test failed");}
 static void SelfTest(string report){
  IntPtr data=Marshal.AllocHGlobal(64),handle=OpenProcess(ReadQuery,false,Process.GetCurrentProcess().Id);Require(handle!=IntPtr.Zero);
  try {
   byte[] bytes=new byte[64];Array.Copy(Sites[0].Expected,bytes,Sites[0].Expected.Length);Marshal.Copy(bytes,0,data,bytes.Length);ulong address=(ulong)data.ToInt64();
   Require(Matches(Read(handle,address,32),Sites[0].Expected));Require(Read(handle,1,32).Length==0);Require(Read(handle,address,65).Length==0);Require(!Supported(new string('0',64)));
   Require(Matches(Encoding.ASCII.GetBytes("Fighter_Disable_StaminaLoss\0"),Encoding.ASCII.GetBytes("Fighter_Disable_StaminaLoss").Take(26).ToArray()));
   bytes[0]=0xe9;Array.Copy(BitConverter.GetBytes(27),0,bytes,1,4);Marshal.Copy(bytes,0,data,bytes.Length);Require(!Matches(Read(handle,address,32),Sites[0].Expected));Require(Jump(handle,address,bytes)==address+32);
   bytes=new byte[64];bytes[0]=0xff;bytes[1]=0x25;Array.Copy(BitConverter.GetBytes(address+32),0,bytes,6,8);Marshal.Copy(bytes,0,data,bytes.Length);Require(Jump(handle,address,bytes)==address+32);
   bytes=new byte[64];bytes[0]=0x48;bytes[1]=0xb8;Array.Copy(BitConverter.GetBytes(address+32),0,bytes,2,8);bytes[10]=0xff;bytes[11]=0xe0;Require(Jump(handle,address,bytes)==address+32);
   File.WriteAllText(report,"PASS: read-only self-process header capture, exact matching, bounds/unreadable rejection, supported-engine guard and three bounded jump decoders. No game process accessed.\r\n");
  }finally{CloseHandle(handle);Marshal.FreeHGlobal(data);}
 }
 [STAThread] static int Main(string[] args){
  bool quiet=args.Length==2&&args[0]=="--out";
  try {
   if(args.Length==2&&args[0]=="--self-test"){SelfTest(args[1]);return 0;}
   if(args.Length!=0&&!quiet)throw new ArgumentException("Usage: run with no arguments, or --out report.txt");
   string folder=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"UFC6 Actions Overlay","SupportReports");
   if(!quiet)Directory.CreateDirectory(folder);
   string report=quiet?Path.GetFullPath(args[1]):Path.Combine(folder,"UFC6-career-code-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".txt");
   File.WriteAllText(report,Inspect(),new UTF8Encoding(false));
   if(!quiet){MessageBox.Show("Read-only check complete. Send the selected TXT file to the mod developer.\r\n\r\n"+report,"UFC 6 career-code check",MessageBoxButtons.OK,MessageBoxIcon.Information);Process.Start("explorer.exe","/select,\""+report+"\"");}
   return 0;
  }catch(Exception error){if(!quiet)MessageBox.Show("Check failed: "+error.GetType().Name+" — "+error.Message,"UFC 6 career-code check",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
 }
}

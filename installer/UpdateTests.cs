// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.IO;
namespace UFC6OverlaySetup {
 public static class UpdateTests {
  static void Require(bool value,string message){if(!value)throw new Exception(message);}
  static void Reject(Action action){try{action();}catch(InvalidDataException){return;}throw new Exception("Unsafe update input was accepted.");}
  static string Fixture(){return "{\"tag_name\":\"v0.9.5\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"UFC6-Actions-Overlay-Setup-0.9.5.exe\",\"size\":10,\"browser_download_url\":\"https://github.com/thyaguster/ufc6-actions-overlay/releases/download/v0.9.5/UFC6-Actions-Overlay-Setup-0.9.5.exe\"},{\"name\":\"SHA256SUMS.txt\",\"size\":100,\"browser_download_url\":\"https://github.com/thyaguster/ufc6-actions-overlay/releases/download/v0.9.5/SHA256SUMS.txt\"}]}";}
  public static string Run(){
   string json=Fixture();Require(UpdateClient.Parse(json,"0.9.4").IsNewer,"New release was not detected.");Require(!UpdateClient.Parse(json,"0.9.5").IsNewer,"Current release offered as update.");Require(!UpdateClient.Parse(json,"0.10.0").IsNewer,"Downgrade offered.");
   Reject(delegate{UpdateClient.Parse(json.Replace("v0.9.5","v../evil"),"0.9.4");});Reject(delegate{UpdateClient.Parse(json.Replace("\"draft\":false","\"draft\":true"),"0.9.4");});Reject(delegate{UpdateClient.Parse(json.Replace("\"prerelease\":false","\"prerelease\":true"),"0.9.4");});
   Reject(delegate{UpdateClient.Parse(json.Replace("https://github.com/thyaguster/","https://github.com/other/"),"0.9.4");});Reject(delegate{UpdateClient.Parse(json.Replace("\"size\":10","\"size\":999999999"),"0.9.4");});Reject(delegate{UpdateClient.Parse(json.Replace("SHA256SUMS.txt","missing.txt"),"0.9.4");});
   Require(UpdateClient.AllowedDownload("https://release-assets.githubusercontent.com/file"),"GitHub CDN rejected.");Require(!UpdateClient.AllowedDownload("http://github.com/file")&&!UpdateClient.AllowedDownload("https://github.com.evil.test/file")&&!UpdateClient.AllowedDownload("https://github.com:444/file")&&!UpdateClient.AllowedDownload("https://user@github.com/file"),"Untrusted download origin accepted.");
   string hash=new string('a',64);Require(UpdateClient.Checksum(hash+"  installer.exe\n","installer.exe")==hash,"Checksum parse failed.");Reject(delegate{UpdateClient.Checksum(hash+"  other.exe","installer.exe");});Reject(delegate{UpdateClient.Checksum(hash+"  installer.exe\n"+hash+"  installer.exe","installer.exe");});Reject(delegate{UpdateClient.Checksum("not-a-hash  installer.exe","installer.exe");});
   string file=Path.GetTempFileName();try{File.WriteAllBytes(file,new byte[]{1,2,3});string actual=UpdateClient.HashFile(file);UpdateClient.Verify(file,actual,3);Reject(delegate{UpdateClient.Verify(file,hash,3);});Reject(delegate{UpdateClient.Verify(file,actual,4);});}finally{File.Delete(file);}
   return "PASS: newer/current/older version handling; stable-only releases; exact repository and asset URLs; missing checksums and oversized installers; trusted HTTPS redirects; checksum ambiguity; corrupted or truncated download rejection.\r\nNo game files changed and no installer was launched.\r\n";
  }
 }
}

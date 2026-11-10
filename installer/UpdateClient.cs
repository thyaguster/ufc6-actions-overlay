// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
namespace UFC6OverlaySetup {
 public sealed class GithubAsset { public string name {get;set;} public string browser_download_url {get;set;} public long size {get;set;} public string digest {get;set;} }
 public sealed class GithubRelease { public string tag_name {get;set;} public bool draft {get;set;} public bool prerelease {get;set;} public GithubAsset[] assets {get;set;} }
 public sealed class OverlayUpdate { public string Tag,Version,InstallerName,InstallerUrl,ChecksumsUrl,Digest; public long Size; public bool IsNewer; public string Page { get {return "https://github.com/thyaguster/ufc6-actions-overlay/releases/tag/"+Tag;} } }
 public static class UpdateClient {
  public const string LatestApi="https://api.github.com/repos/thyaguster/ufc6-actions-overlay/releases/latest";
  public const string LatestPage="https://github.com/thyaguster/ufc6-actions-overlay/releases/latest";
  const long MaxInstaller=128L*1024*1024;
  public static Version ParseVersion(string value){
   if(value==null||!Regex.IsMatch(value,@"^v?\d{1,5}\.\d{1,5}\.\d{1,5}$"))throw new InvalidDataException("Unrecognized release version.");
   return new Version(value.TrimStart('v'));
  }
  public static OverlayUpdate Parse(string json,string installed){
   var release=new JavaScriptSerializer {MaxJsonLength=4*1024*1024}.Deserialize<GithubRelease>(json);
   if(release==null||release.draft||release.prerelease||release.assets==null)throw new InvalidDataException("No supported stable release was returned by GitHub.");
   Version version=ParseVersion(release.tag_name);var result=new OverlayUpdate {Tag=release.tag_name,Version=version.ToString(3),IsNewer=version>ParseVersion(installed)};
   result.InstallerName="UFC6-Actions-Overlay-Setup-"+result.Version+".exe";
   string prefix="https://github.com/thyaguster/ufc6-actions-overlay/releases/download/"+result.Tag+"/";
   int installers=0,checksums=0;
   foreach(var asset in release.assets){
    if(asset==null)continue;
    if(asset.name==result.InstallerName){
     installers++;if(asset.browser_download_url!=prefix+result.InstallerName||asset.size<=0||asset.size>MaxInstaller)throw new InvalidDataException("The release installer URL or size is invalid.");
     result.InstallerUrl=asset.browser_download_url;result.Size=asset.size;
     if(!string.IsNullOrEmpty(asset.digest)){
      if(!Regex.IsMatch(asset.digest,@"^sha256:[a-fA-F0-9]{64}$"))throw new InvalidDataException("The release installer digest is invalid.");
      result.Digest=asset.digest.Substring(7).ToLowerInvariant();
     }
    }
    if(asset.name=="SHA256SUMS.txt"){
     checksums++;if(asset.browser_download_url!=prefix+"SHA256SUMS.txt"||asset.size<=0||asset.size>1024*1024)throw new InvalidDataException("The release checksum URL or size is invalid.");
     result.ChecksumsUrl=asset.browser_download_url;
    }
   }
   if(installers!=1||checksums!=1)throw new InvalidDataException("The release needs exactly one matching installer and checksum file.");
   return result;
  }
  public static string Checksum(string text,string filename){
   string found=null;
   using(var reader=new StringReader(text))for(string line;(line=reader.ReadLine())!=null;){
    if(string.IsNullOrWhiteSpace(line))continue;
    var match=Regex.Match(line.Trim(),@"^([a-fA-F0-9]{64})\s+\*?([^\s]+)$");
    if(!match.Success)throw new InvalidDataException("The release checksum file is invalid.");
    if(match.Groups[2].Value==filename){if(found!=null)throw new InvalidDataException("The installer checksum is ambiguous.");found=match.Groups[1].Value.ToLowerInvariant();}
   }
   if(found==null)throw new InvalidDataException("No checksum was provided for this installer.");return found;
  }
  static bool Allowed(Uri uri){return uri.Scheme=="https"&&uri.IsDefaultPort&&string.IsNullOrEmpty(uri.UserInfo)&&(uri.Host=="api.github.com"||uri.Host=="github.com"||uri.Host=="release-assets.githubusercontent.com"||uri.Host=="objects.githubusercontent.com");}
  public static bool AllowedDownload(string url){Uri uri;return Uri.TryCreate(url,UriKind.Absolute,out uri)&&Allowed(uri);}
  static void Fetch(string url,Stream target,long limit){
   ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
   var uri=new Uri(url);
   for(int redirect=0;redirect<6;redirect++){
    if(!Allowed(uri))throw new InvalidDataException("The download redirected outside GitHub.");
    var request=(HttpWebRequest)WebRequest.Create(uri);request.AllowAutoRedirect=false;request.Timeout=15000;request.ReadWriteTimeout=30000;request.UserAgent="UFC6-Actions-Overlay-Updater";
    request.Accept="application/vnd.github+json";
    using(var response=(HttpWebResponse)request.GetResponse()){
     int status=(int)response.StatusCode;
     if(status==301||status==302||status==303||status==307||status==308){string location=response.Headers["Location"];if(string.IsNullOrEmpty(location))throw new IOException("GitHub returned an invalid redirect.");uri=new Uri(uri,location);continue;}
     if(status!=200||response.ContentLength>limit)throw new IOException("GitHub returned an invalid download response.");
     long total=0;byte[] buffer=new byte[65536];using(var input=response.GetResponseStream())for(int n;(n=input.Read(buffer,0,buffer.Length))>0;){total+=n;if(total>limit)throw new IOException("The download exceeds its size limit.");target.Write(buffer,0,n);}return;
    }
   }
   throw new IOException("Too many redirects from GitHub.");
  }
  static string Text(string url,int limit){using(var memory=new MemoryStream()){Fetch(url,memory,limit);return Encoding.UTF8.GetString(memory.ToArray()).TrimStart('\ufeff');}}
  public static OverlayUpdate Latest(string installed){return Parse(Text(LatestApi,4*1024*1024),installed);}
  public static string HashFile(string path){using(var input=File.OpenRead(path))using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(input)).Replace("-","").ToLowerInvariant();}
  public static void Verify(string path,string expected,long size){if(new FileInfo(path).Length!=size||HashFile(path)!=expected)throw new InvalidDataException("The installer failed SHA-256 verification. It will not be opened.");}
  public static string Download(OverlayUpdate release,string root){
   string expected=Checksum(Text(release.ChecksumsUrl,1024*1024),release.InstallerName);
   if(release.Digest!=null&&release.Digest!=expected)throw new InvalidDataException("The GitHub asset digest and checksum file disagree.");
   string folder=Path.Combine(Path.GetFullPath(root),release.Tag);Directory.CreateDirectory(folder);
   string final=Path.Combine(folder,release.InstallerName),temp=final+"."+Guid.NewGuid().ToString("N")+".partial";
   if(File.Exists(final)){try{Verify(final,expected,release.Size);return final;}catch(InvalidDataException){}}
   try{
    using(var output=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){Fetch(release.InstallerUrl,output,release.Size);output.Flush(true);}
    Verify(temp,expected,release.Size);
    if(File.Exists(final))File.Delete(final);File.Move(temp,final);return final;
   }finally{if(File.Exists(temp))File.Delete(temp);}
  }
 }
}

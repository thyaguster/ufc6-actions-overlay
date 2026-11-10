// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
namespace UFC6OverlaySetup {
 public sealed class UpdateForm:ModernForm {
  readonly string installed,folder;
  readonly TextBox status=new TextBox();
  readonly ModernButton check=new ModernButton(),download=new ModernButton(),details=new ModernButton();
  OverlayUpdate release;
  public bool InstallerOpened {get;private set;}
  public UpdateForm(string installed,string folder,bool automatic=true){
   this.installed=installed;this.folder=folder;
   Text="UFC 6 Actions Overlay Updates";ClientSize=new Size(680,470);
   var title=ModernUi.Label("Overlay updates",110,70,530,40,22,true);
   var current=ModernUi.Label("Installed version  v"+installed,112,116,530,24,10,false,true);
   var info=new ModernCard {Bounds=new Rectangle(28,164,624,172)};
   info.Controls.Add(ModernUi.Label("Latest release",20,16,584,27,12,true));
   status.SetBounds(20,52,584,100);ModernUi.Status(status);
   status.Text="Check the latest stable release on GitHub.\r\nClose UFC 6 and its launcher before installing an update.\r\nYour settings, screenshots and save backups are kept.";info.Controls.Add(status);
   check.Text="Check for updates";check.SetBounds(28,358,168,44);
   details.Text="Release page";details.SetBounds(208,358,130,44);
   download.Text="Download && open installer";download.SetBounds(350,358,302,44);download.Tone=ButtonTone.Primary;download.Enabled=false;
   var hint=ModernUi.Label("Downloads are verified before the installer opens.",28,424,624,24,9,false,true);
   Controls.AddRange(new Control[]{ModernUi.Logo(28,66,64),title,current,info,check,details,download,hint});
   check.Click+=delegate{Check();};details.Click+=delegate{Process.Start(new ProcessStartInfo(release==null?UpdateClient.LatestPage:release.Page){UseShellExecute=true});};download.Click+=delegate{Download();};
   if(automatic)Shown+=delegate{Check();};
  }
  void Check(){
   check.Enabled=download.Enabled=false;status.Text="Checking GitHub...";
   var worker=new BackgroundWorker();worker.DoWork+=delegate(object sender,DoWorkEventArgs e){e.Result=UpdateClient.Latest(installed);};
   worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e){
    if(IsDisposed){worker.Dispose();return;}check.Enabled=true;
    if(e.Error!=null){release=null;status.Text="Update check failed: "+e.Error.Message+"\r\nTry again later, or open the release page.";}
    else{release=(OverlayUpdate)e.Result;download.Enabled=release.IsNewer;status.Text=release.IsNewer?"Version v"+release.Version+" is available.\r\nDownload the verified installer, then close UFC 6 and its launcher before clicking Install / Update.\r\nYour selected game folder will be filled in automatically.":UpdateClient.ParseVersion(installed)>UpdateClient.ParseVersion(release.Version)?"Installed v"+installed+" is newer than the latest public release v"+release.Version+".":"You are up to date. Latest version: v"+release.Version+".";}
    worker.Dispose();
   };worker.RunWorkerAsync();
  }
  void Download(){
   if(release==null||!release.IsNewer)return;check.Enabled=download.Enabled=false;status.Text="Downloading and verifying v"+release.Version+"...";
   var worker=new BackgroundWorker();worker.DoWork+=delegate(object sender,DoWorkEventArgs e){e.Result=UpdateClient.Download(release,Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"UFC6 Actions Overlay","Updates"));};
   worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e){
    if(IsDisposed){worker.Dispose();return;}check.Enabled=true;download.Enabled=true;
    if(e.Error!=null)status.Text="Update download failed: "+e.Error.Message+"\r\nThe installer was not opened.";
    else try{
     string path=(string)e.Result;string arguments="";
     if(!string.IsNullOrWhiteSpace(folder)){string full=Path.GetFullPath(folder);if(full.IndexOf('"')>=0)throw new IOException("Invalid game folder.");arguments="--folder \""+full+(full.EndsWith("\\")?"\\":"")+"\"";}
     Process.Start(new ProcessStartInfo(path,arguments){UseShellExecute=true,WorkingDirectory=Path.GetDirectoryName(path)});InstallerOpened=true;Close();
    }catch(Exception error){status.Text="Cannot open the installer: "+error.Message;}
    worker.Dispose();
   };worker.RunWorkerAsync();
  }
 }
}

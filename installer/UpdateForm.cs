// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
namespace UFC6OverlaySetup {
 public sealed class UpdateForm:Form {
  readonly string installed,folder;
  readonly TextBox status=new TextBox();
  readonly Button check=new Button(),download=new Button(),details=new Button();
  OverlayUpdate release;
  public bool InstallerOpened {get;private set;}
  public UpdateForm(string installed,string folder,bool automatic=true){
   this.installed=installed;this.folder=folder;
   Text="UFC 6 Actions Overlay Updates";Font=new Font("Segoe UI",10);ClientSize=new Size(620,310);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterParent;AutoScaleMode=AutoScaleMode.Dpi;
   Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath);
   var title=new Label {Text="Overlay updates",Font=new Font("Segoe UI",18,FontStyle.Bold),Bounds=new Rectangle(24,18,560,40)};
   var current=new Label {Text="Installed version: v"+installed,Bounds=new Rectangle(26,64,560,26)};
   status.SetBounds(26,100,568,108);status.Multiline=true;status.ReadOnly=true;status.BackColor=SystemColors.Window;status.ScrollBars=ScrollBars.Vertical;
   status.Text="Check the latest stable release on GitHub.\r\nClose UFC 6 and its launcher before installing an update.\r\nYour settings, screenshots and save backups are kept.";
   check.Text="Check for updates";check.SetBounds(26,225,160,38);
   details.Text="Release page";details.SetBounds(198,225,130,38);
   download.Text="Download && open installer";download.SetBounds(340,225,254,38);download.Enabled=false;
   var hint=new Label {Text="Downloads are verified before the installer opens.",Bounds=new Rectangle(26,276,568,24)};
   Controls.AddRange(new Control[]{title,current,status,check,details,download,hint});
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

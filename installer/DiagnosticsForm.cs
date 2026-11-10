// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
namespace UFC6OverlaySetup {
 public sealed class DiagnosticsForm:ModernForm {
  readonly string folder,version,renderer;readonly int pid;
  readonly TextBox status=new TextBox();readonly ModernButton export=new ModernButton(),open=new ModernButton();
  string saved;bool busy;
  public DiagnosticsForm(string version,string folder,string renderer="unknown",int pid=0){
   this.version=version;this.folder=folder;this.renderer=renderer;this.pid=pid;
   Text="UFC 6 Actions Overlay Support";ClientSize=new Size(680,470);
   var card=new ModernCard{Bounds=new Rectangle(28,164,624,172)};
   card.Controls.Add(ModernUi.Label("Create a support report",20,16,584,27,12,true));
   status.SetBounds(20,52,584,100);ModernUi.Status(status);
   status.Text="Collect overlay logs, component checks and renderer status in a ZIP.\r\nPersonal paths are removed. Review the report before sharing.\r\nNo save files or game content are included; nothing is uploaded.";card.Controls.Add(status);
   export.Text="Export support report";export.SetBounds(28,358,294,44);export.Tone=ButtonTone.Primary;
   open.Text="Show report in folder";open.SetBounds(338,358,314,44);open.Enabled=false;
   Controls.AddRange(new Control[]{ModernUi.Logo(28,66,64),ModernUi.Label("Overlay diagnostics",110,70,530,40,22,true),ModernUi.Label("Support tools  v"+version,112,116,530,24,10,false,true),card,export,open,ModernUi.Label("Reports: Documents / UFC6 Actions Overlay / SupportReports",28,424,624,24,9,false,true)});
   export.Click+=delegate{Export();};open.Click+=delegate{if(saved!=null&&File.Exists(saved))Process.Start(new ProcessStartInfo("explorer.exe","/select,\""+saved+"\""){UseShellExecute=true});};
   FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy)e.Cancel=true;};
  }
  void Export(){
   busy=true;export.Enabled=open.Enabled=false;status.Text="Collecting and verifying the report...";
   var worker=new BackgroundWorker();
   worker.DoWork+=delegate(object sender,DoWorkEventArgs e){string documents=Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);if(string.IsNullOrEmpty(documents))throw new IOException("Windows Documents folder is unavailable.");e.Result=Diagnostics.Export(folder,Path.Combine(documents,"UFC6 Actions Overlay","SupportReports"),version,renderer,pid);};
   worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e){busy=false;export.Enabled=true;if(e.Error!=null)status.Text="Could not create the report: "+e.Error.Message;else{saved=(string)e.Result;open.Enabled=true;status.Text="Support report saved and verified.\r\n"+saved+"\r\nReview the ZIP, then share it with your support request.";}worker.Dispose();};worker.RunWorkerAsync();
  }
 }
}

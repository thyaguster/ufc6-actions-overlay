// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;
using System.Windows.Forms;
namespace UFC6OverlaySetup {
 public static class UpdateProgram {
  [STAThread] public static int Main(string[] args){
   string version=Assembly.GetExecutingAssembly().GetName().Version.ToString(3);
   try{
    if(args.Length==2&&args[0]=="--self-test"){File.WriteAllText(args[1],UpdateTests.Run());return 0;}
    if(args.Length==2&&args[0]=="--check-only"){var result=UpdateClient.Latest(version);File.WriteAllText(args[1],new JavaScriptSerializer().Serialize(new{installed=version,latest=result.Version,available=result.IsNewer,page=result.Page}));return 0;}
    if(args.Length==2&&args[0]=="--download-only"){var result=UpdateClient.Latest(version);string path=UpdateClient.Download(result,args[1]);File.WriteAllText(Path.Combine(args[1],"download-result.txt"),path);return 0;}
    Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
    if(args.Length==2&&args[0]=="--preview"){using(var form=new UpdateForm(version,"",false)){form.Show();Application.DoEvents();using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(Point.Empty,image.Size));image.Save(args[1]);}}return 0;}
    string folder="";if(args.Length==2&&args[0]=="--folder")folder=Path.GetFullPath(args[1]);else if(args.Length!=0)throw new ArgumentException("Use --folder with the game folder.");
    Application.Run(new UpdateForm(version,folder));return 0;
   }catch(Exception error){if(args.Length==2&&(args[0]=="--self-test"||args[0]=="--check-only"))File.WriteAllText(args[1],error.ToString());else if(args.Length==2&&args[0]=="--download-only"){Directory.CreateDirectory(args[1]);File.WriteAllText(Path.Combine(args[1],"download-error.txt"),error.ToString());}else MessageBox.Show(error.Message,"Overlay update error",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
  }
 }
}

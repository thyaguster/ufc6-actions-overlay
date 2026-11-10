// Original U6 vector monogram. Run with --output <assets folder> after compilation.
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
class MakeIcon {
 static GraphicsPath Rounded(float x,float y,float w,float h,float r){var p=new GraphicsPath();p.AddArc(x,y,r*2,r*2,180,90);p.AddArc(x+w-r*2,y,r*2,r*2,270,90);p.AddArc(x+w-r*2,y+h-r*2,r*2,r*2,0,90);p.AddArc(x,y+h-r*2,r*2,r*2,90,90);p.CloseFigure();return p;}
 static void Main(string[] args){
  string folder=args[1];Directory.CreateDirectory(folder);int[] sizes={16,24,32,48,64,128,256};var images=new byte[sizes.Length][];
  using(var master=new Bitmap(1024,1024,PixelFormat.Format32bppArgb)){
   using(var g=Graphics.FromImage(master)){
    g.SmoothingMode=SmoothingMode.AntiAlias;g.ScaleTransform(4,4);g.Clear(Color.Transparent);
    using(var p=Rounded(5,5,246,246,55))using(var b=new SolidBrush(Color.FromArgb(17,22,30)))g.FillPath(b,p);
    using(var p=Rounded(14,14,228,228,47))using(var pen=new Pen(Color.FromArgb(52,68,81),4))g.DrawPath(pen,p);
    using(var pen=new Pen(Color.FromArgb(241,246,248),17)){
     pen.StartCap=pen.EndCap=LineCap.Round;pen.LineJoin=LineJoin.Round;
     using(var p=new GraphicsPath()){p.AddLine(51,75,51,144);p.AddBezier(51,144,51,184,111,184,111,144);p.AddLine(111,144,111,75);g.DrawPath(pen,p);}
     using(var p=new GraphicsPath()){p.AddLine(195,78,171,78);p.AddBezier(171,78,149,78,139,97,139,123);p.AddLine(139,123,139,146);p.AddBezier(139,146,139,185,197,185,197,146);p.AddBezier(197,146,197,126,187,118,168,118);p.AddLine(168,118,141,118);g.DrawPath(pen,p);}
    }
    using(var p=Rounded(145,181,79,42,21))using(var b=new SolidBrush(Color.FromArgb(118,224,148)))g.FillPath(b,p);
    using(var pen=new Pen(Color.FromArgb(18,49,33),6)){pen.StartCap=pen.EndCap=LineCap.Round;pen.LineJoin=LineJoin.Round;g.DrawLines(pen,new[]{new Point(177,193),new Point(187,202),new Point(177,211)});}
   }
   for(int i=0;i<sizes.Length;i++)using(var frame=new Bitmap(sizes[i],sizes[i],PixelFormat.Format32bppArgb)){
    using(var g=Graphics.FromImage(frame)){g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.DrawImage(master,new Rectangle(0,0,sizes[i],sizes[i]));}
    using(var stream=new MemoryStream()){frame.Save(stream,ImageFormat.Png);images[i]=stream.ToArray();}
    if(sizes[i]==256)frame.Save(Path.Combine(folder,"overlay.png"),ImageFormat.Png);
   }
  }
  using(var file=File.Create(Path.Combine(folder,"overlay.ico")))using(var w=new BinaryWriter(file)){
   w.Write((ushort)0);w.Write((ushort)1);w.Write((ushort)sizes.Length);int offset=6+16*sizes.Length;
   for(int i=0;i<sizes.Length;i++){w.Write((byte)(sizes[i]==256?0:sizes[i]));w.Write((byte)(sizes[i]==256?0:sizes[i]));w.Write((byte)0);w.Write((byte)0);w.Write((ushort)1);w.Write((ushort)32);w.Write(images[i].Length);w.Write(offset);offset+=images[i].Length;}
   foreach(var data in images)w.Write(data);
  }
 }
}

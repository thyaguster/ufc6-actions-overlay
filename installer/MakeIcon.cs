// UFC 6 wordmark icon with outlined lettering. Run with --output <assets folder> after compilation.
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
class MakeIcon {
 static GraphicsPath Rounded(float x,float y,float w,float h,float r){var p=new GraphicsPath();p.AddArc(x,y,r*2,r*2,180,90);p.AddArc(x+w-r*2,y,r*2,r*2,270,90);p.AddArc(x+w-r*2,y+h-r*2,r*2,r*2,0,90);p.AddArc(x,y+h-r*2,r*2,r*2,90,90);p.CloseFigure();return p;}
 static GraphicsPath FitText(string value,FontFamily family,FontStyle style,RectangleF box){
  var p=new GraphicsPath();p.AddString(value,family,(int)style,100,PointF.Empty,StringFormat.GenericTypographic);var bounds=p.GetBounds();
  using(var transform=new Matrix()){transform.Translate(-bounds.X,-bounds.Y,MatrixOrder.Append);transform.Scale(box.Width/bounds.Width,box.Height/bounds.Height,MatrixOrder.Append);transform.Translate(box.X,box.Y,MatrixOrder.Append);p.Transform(transform);}return p;
 }
 static string Svg(GraphicsPath p){
  var text=new System.Text.StringBuilder();var points=p.PathPoints;var types=p.PathTypes;
  Func<float,string> n=delegate(float x){return x.ToString("0.###",System.Globalization.CultureInfo.InvariantCulture);};
  for(int i=0;i<points.Length;i++){
   int type=types[i]&7;
   if(type==0)text.Append("M"+n(points[i].X)+" "+n(points[i].Y));
   else if(type==1)text.Append("L"+n(points[i].X)+" "+n(points[i].Y));
   else if(type==3){text.Append("C"+n(points[i].X)+" "+n(points[i].Y)+" "+n(points[i+1].X)+" "+n(points[i+1].Y)+" "+n(points[i+2].X)+" "+n(points[i+2].Y));i+=2;}
   if((types[i]&128)!=0)text.Append("Z");
  }return text.ToString();
 }
 static void Main(string[] args){
  string folder=args[1];Directory.CreateDirectory(folder);int[] sizes={16,24,32,48,64,128,256};var images=new byte[sizes.Length][];
  using(var master=new Bitmap(1024,1024,PixelFormat.Format32bppArgb)){
   using(var g=Graphics.FromImage(master)){
    g.SmoothingMode=SmoothingMode.AntiAlias;g.ScaleTransform(4,4);g.Clear(Color.Transparent);
    using(var p=Rounded(5,5,246,246,55))using(var b=new SolidBrush(Color.FromArgb(17,22,30)))g.FillPath(b,p);
    using(var p=Rounded(14,14,228,228,47))using(var pen=new Pen(Color.FromArgb(52,68,81),4))g.DrawPath(pen,p);
    using(var family=new FontFamily("Segoe UI")){
     using(var word=FitText("UFC",family,FontStyle.Bold|FontStyle.Italic,new RectangleF(28,40,200,63)))
     using(var number=FitText("6",family,FontStyle.Bold|FontStyle.Italic,new RectangleF(83,116,96,105)))
     using(var white=new SolidBrush(Color.FromArgb(246,248,250)))
     using(var red=new SolidBrush(Color.FromArgb(236,36,55))){
      g.FillPath(white,word);g.FillPath(red,number);
      string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 256 256\"><rect x=\"5\" y=\"5\" width=\"246\" height=\"246\" rx=\"55\" fill=\"#11161e\"/><rect x=\"14\" y=\"14\" width=\"228\" height=\"228\" rx=\"47\" fill=\"none\" stroke=\"#344451\" stroke-width=\"4\"/><path fill=\"#f6f8fa\" d=\""+Svg(word)+"\"/><path fill=\"#ec2437\" d=\""+Svg(number)+"\"/></svg>";
      File.WriteAllText(Path.Combine(folder,"overlay.svg"),svg+Environment.NewLine);
     }
    }
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

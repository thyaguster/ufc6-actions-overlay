// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;
namespace UFC6OverlaySetup {
 public static class ModernUi {
  public static readonly Color Background=Color.FromArgb(14,20,29),Card=Color.FromArgb(24,34,47),Text=Color.FromArgb(239,245,251),Muted=Color.FromArgb(147,166,186),Border=Color.FromArgb(48,66,85),Accent=Color.FromArgb(118,224,148);
  static Image brand;
  public static Image Brand {get {if(brand==null)using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("overlay-brand.png")){if(stream!=null)using(var image=Image.FromStream(stream))brand=new Bitmap(image);}return brand;}}
  public static GraphicsPath Rounded(RectangleF box,float radius){
   float d=Math.Min(radius*2,Math.Min(box.Width,box.Height));var path=new GraphicsPath();
   path.AddArc(box.X,box.Y,d,d,180,90);path.AddArc(box.Right-d,box.Y,d,d,270,90);path.AddArc(box.Right-d,box.Bottom-d,d,d,0,90);path.AddArc(box.X,box.Bottom-d,d,d,90,90);path.CloseFigure();return path;
  }
  public static Label Label(string text,int x,int y,int width,int height,float size=10,bool bold=false,bool muted=false){return new Label {Text=text,Bounds=new Rectangle(x,y,width,height),Font=new Font("Segoe UI",size,bold?FontStyle.Bold:FontStyle.Regular),ForeColor=muted?Muted:Text,BackColor=Color.Transparent};}
  public static PictureBox Logo(int x,int y,int size){return new PictureBox {Image=Brand,SizeMode=PictureBoxSizeMode.Zoom,Bounds=new Rectangle(x,y,size,size),BackColor=Color.Transparent};}
  [DllImport("uxtheme.dll",CharSet=CharSet.Unicode)]static extern int SetWindowTheme(IntPtr window,string subApp,string subId);
  public static void Status(TextBox box){
   box.TabStop=false;box.Multiline=true;box.ReadOnly=true;box.WordWrap=true;box.BorderStyle=BorderStyle.None;box.BackColor=Card;box.ForeColor=Text;box.Font=new Font("Segoe UI",10);box.ScrollBars=ScrollBars.None;
   box.HandleCreated+=delegate{SetWindowTheme(box.Handle,"DarkMode_Explorer",null);};
   box.TextChanged+=delegate{int height=TextRenderer.MeasureText(box.Text,box.Font,new Size(Math.Max(1,box.Width-20),int.MaxValue),TextFormatFlags.WordBreak|TextFormatFlags.TextBoxControl).Height;box.ScrollBars=height>box.Height?ScrollBars.Vertical:ScrollBars.None;};
  }
 }
 public class ModernCard:Panel {
  public ModernCard(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);BackColor=ModernUi.Background;}
  protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(Parent is ModernCard?ModernUi.Card:Parent==null?ModernUi.Background:Parent.BackColor);}
  protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);if(Width<2||Height<2)return;e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;using(var p=ModernUi.Rounded(new RectangleF(.5f,.5f,Width-1,Height-1),14))using(var fill=new SolidBrush(ModernUi.Card))using(var pen=new Pen(ModernUi.Border)){e.Graphics.FillPath(fill,p);e.Graphics.DrawPath(pen,p);}}
 }
 public enum ButtonTone {Secondary,Primary,Danger,Chrome}
 public class ModernButton:Button {
  public ButtonTone Tone {get;set;}
  bool hover,pressed;
  public ModernButton(){FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;UseVisualStyleBackColor=false;Font=new Font("Segoe UI",10,FontStyle.Bold);Cursor=Cursors.Hand;SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);}
  protected override void OnMouseEnter(EventArgs e){hover=true;base.OnMouseEnter(e);Invalidate();}
  protected override void OnMouseLeave(EventArgs e){hover=false;pressed=false;base.OnMouseLeave(e);Invalidate();}
  protected override void OnMouseDown(MouseEventArgs e){pressed=true;base.OnMouseDown(e);Invalidate();}
  protected override void OnMouseUp(MouseEventArgs e){pressed=false;base.OnMouseUp(e);Invalidate();}
  protected override void OnGotFocus(EventArgs e){base.OnGotFocus(e);Invalidate();}
  protected override void OnLostFocus(EventArgs e){base.OnLostFocus(e);Invalidate();}
  protected override void OnEnabledChanged(EventArgs e){base.OnEnabledChanged(e);Invalidate();}
  protected override void OnPaint(PaintEventArgs e){
   if(Width<2||Height<2)return;e.Graphics.Clear(Parent is ModernCard?ModernUi.Card:Parent==null?ModernUi.Background:Parent.BackColor);e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;
   Color fill=Color.FromArgb(34,49,66),text=ModernUi.Text,border=Color.FromArgb(58,78,99);
   if(Tone==ButtonTone.Primary){fill=ModernUi.Accent;text=Color.FromArgb(15,43,26);border=fill;}
   if(Tone==ButtonTone.Danger){fill=Color.FromArgb(45,30,39);text=Color.FromArgb(242,161,171);border=Color.FromArgb(79,47,61);}
   if(Tone==ButtonTone.Chrome){fill=ModernUi.Background;text=ModernUi.Muted;border=fill;}
   if(hover&&Enabled)fill=ControlPaint.Light(fill,.12f);if(pressed&&Enabled)fill=ControlPaint.Dark(fill,.12f);
   if(!Enabled){fill=Color.FromArgb(26,37,49);text=Color.FromArgb(100,119,137);border=Color.FromArgb(36,51,66);}
   if(Focused&&ShowFocusCues)border=ModernUi.Accent;
   using(var p=ModernUi.Rounded(new RectangleF(.75f,.75f,Width-1.5f,Height-1.5f),9))using(var b=new SolidBrush(fill))using(var pen=new Pen(border,1.3f)){e.Graphics.FillPath(b,p);e.Graphics.DrawPath(pen,p);}
   TextRenderer.DrawText(e.Graphics,Text,Font,ClientRectangle,text,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis);
  }
 }
 public class ModernForm:Form {
  readonly Label caption=new Label();readonly Panel chrome=new Panel();
  [DllImport("user32.dll")]static extern bool ReleaseCapture();
  [DllImport("user32.dll")]static extern IntPtr SendMessage(IntPtr window,int message,IntPtr wp,IntPtr lp);
  public ModernForm(){
   Font=new Font("Segoe UI",10);BackColor=ModernUi.Background;ForeColor=ModernUi.Text;FormBorderStyle=FormBorderStyle.None;StartPosition=FormStartPosition.CenterScreen;AutoScaleMode=AutoScaleMode.Dpi;MaximizeBox=false;DoubleBuffered=true;
   Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath);
   chrome.SetBounds(1,1,ClientSize.Width-2,43);chrome.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;chrome.BackColor=ModernUi.Background;
   var logo=ModernUi.Logo(14,11,22);caption.SetBounds(44,11,ClientSize.Width-158,24);caption.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;caption.ForeColor=ModernUi.Muted;caption.Font=new Font("Segoe UI",9);caption.TextAlign=ContentAlignment.MiddleLeft;
   var minimize=new ModernButton {Text="—",Tone=ButtonTone.Chrome,AccessibleName="Minimize",TabStop=false,Bounds=new Rectangle(ClientSize.Width-100,5,42,32),Anchor=AnchorStyles.Top|AnchorStyles.Right};
   var close=new ModernButton {Text="×",Tone=ButtonTone.Chrome,AccessibleName="Close",TabStop=false,Font=new Font("Segoe UI",14),Bounds=new Rectangle(ClientSize.Width-53,5,42,32),Anchor=AnchorStyles.Top|AnchorStyles.Right};
   minimize.Click+=delegate{WindowState=FormWindowState.Minimized;};close.Click+=delegate{Close();};
   MouseEventHandler drag=delegate(object sender,MouseEventArgs e){if(e.Button==MouseButtons.Left){ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);}};
   chrome.MouseDown+=drag;caption.MouseDown+=drag;logo.MouseDown+=drag;chrome.Controls.AddRange(new Control[]{logo,caption,minimize,close});Controls.Add(chrome);
  }
  protected override void OnTextChanged(EventArgs e){base.OnTextChanged(e);if(caption!=null)caption.Text=Text;}
  protected override void OnSizeChanged(EventArgs e){base.OnSizeChanged(e);if(Width>0&&Height>0){var old=Region;using(var p=ModernUi.Rounded(new RectangleF(0,0,Width,Height),18))Region=new Region(p);if(old!=null)old.Dispose();}}
  protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);if(Width<2||Height<2)return;e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;using(var p=ModernUi.Rounded(new RectangleF(.5f,.5f,Width-1,Height-1),18))using(var pen=new Pen(ModernUi.Border))e.Graphics.DrawPath(pen,p);}
 }
}

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Starshine {
 public static class Art {
  public static Color Text=ColorTranslator.FromHtml("#F7ECFF"), Muted=ColorTranslator.FromHtml("#D4BDD9");
  public static GraphicsPath Round(RectangleF r,float radius){
   var p=new GraphicsPath();float d=Math.Min(radius*2,Math.Min(r.Width,r.Height));
   p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.Right-d,r.Y,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.X,r.Bottom-d,d,d,90,90);p.CloseFigure();return p;
  }
  public static void Star(Graphics g,float x,float y,float r,Color color){
   using(var p=new GraphicsPath()){
    p.AddBezier(x,y-r,x+r*.13f,y-r*.22f,x+r*.22f,y-r*.13f,x+r,y);
    p.AddBezier(x+r,y,x+r*.22f,y+r*.13f,x+r*.13f,y+r*.22f,x,y+r);
    p.AddBezier(x,y+r,x-r*.13f,y+r*.22f,x-r*.22f,y+r*.13f,x-r,y);
    p.AddBezier(x-r,y,x-r*.22f,y-r*.13f,x-r*.13f,y-r*.22f,x,y-r);p.CloseFigure();
    using(var b=new SolidBrush(color))g.FillPath(b,p);
   }
  }
  public static void Icon(Graphics g,string icon,RectangleF r,Color color){
   var state=g.Save();g.TranslateTransform(r.X,r.Y);g.ScaleTransform(r.Width/24,r.Height/24);
   using(var pen=new Pen(color,1.6f))using(var brush=new SolidBrush(color)){
    pen.StartCap=pen.EndCap=LineCap.Round;pen.LineJoin=LineJoin.Round;
    switch(icon){
     case "record":g.DrawEllipse(pen,3,3,18,18);g.FillEllipse(brush,7,7,10,10);break;
     case "run":g.FillPolygon(brush,new[]{new PointF(6,3),new PointF(21,12),new PointF(6,21)});break;
     case "stop":using(var p=Round(new RectangleF(4,4,16,16),3))g.FillPath(brush,p);break;
     case "refresh":g.DrawArc(pen,3,3,18,18,40,280);g.DrawLines(pen,new[]{new PointF(21,3),new PointF(21,9),new PointF(15,9)});break;
     case "folder":g.DrawPolygon(pen,new[]{new PointF(2,6),new PointF(9,6),new PointF(12,9),new PointF(22,9),new PointF(22,21),new PointF(2,21)});break;
     case "prices":g.DrawLines(pen,new[]{new PointF(3,20),new PointF(9,13),new PointF(13,16),new PointF(21,5)});g.DrawLines(pen,new[]{new PointF(15,5),new PointF(21,5),new PointF(21,11)});break;
     case "layers":g.DrawPolygon(pen,new[]{new PointF(2,8),new PointF(12,2),new PointF(22,8),new PointF(12,14)});g.DrawLines(pen,new[]{new PointF(2,13),new PointF(12,19),new PointF(22,13)});break;
     case "upgrade":g.DrawLines(pen,new[]{new PointF(4,12),new PointF(12,4),new PointF(20,12)});g.DrawLines(pen,new[]{new PointF(4,21),new PointF(12,13),new PointF(20,21)});break;
     case "min":g.DrawLine(pen,5,12,19,12);break;
     case "max":g.DrawRectangle(pen,6,6,12,12);break;
     case "close":g.DrawLine(pen,6,6,18,18);g.DrawLine(pen,18,6,6,18);break;
     default:g.DrawEllipse(pen,3,3,18,18);g.DrawLine(pen,12,11,12,17);g.FillEllipse(brush,11,6,2,2);break;
    }
   }g.Restore(state);
  }
 }
 public sealed class ShineButton : Button {
  public string Symbol {get;set;} public bool Primary {get;set;} public bool Chrome {get;set;}
  bool over,pressed;float blend;readonly Timer animation=new Timer();
  public ShineButton(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;ForeColor=Art.Text;FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;Cursor=Cursors.Hand;animation.Interval=15;animation.Tick+=delegate{float target=over?1:0;blend+=Math.Sign(target-blend)*Math.Min(.12f,Math.Abs(target-blend));Invalidate();if(Math.Abs(target-blend)<.01f)animation.Stop();};}
  protected override void OnMouseEnter(EventArgs e){base.OnMouseEnter(e);over=true;animation.Start();}
  protected override void OnMouseLeave(EventArgs e){base.OnMouseLeave(e);over=false;pressed=false;animation.Start();}
  protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);pressed=true;Invalidate();}
  protected override void OnMouseUp(MouseEventArgs e){base.OnMouseUp(e);pressed=false;Invalidate();}
  protected override void OnEnabledChanged(EventArgs e){base.OnEnabledChanged(e);Invalidate();}
  protected override void OnPaintBackground(PaintEventArgs e){
   // ButtonBase paints native hover/pressed backgrounds even for an owner-drawn
   // button. Always restore the actual form surface, including rounded corners.
   var form=Parent as ShineForm;
   if(form==null){base.OnPaintBackground(e);return;}
   var state=e.Graphics.Save();
   e.Graphics.TranslateTransform(-Left,-Top);
   form.PaintSurface(e.Graphics);
   e.Graphics.Restore(state);
  }
  protected override void OnPaint(PaintEventArgs e){
   OnPaintBackground(e);
   var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;float scale=Font.Size/11f;
   var r=new RectangleF(1,1,Width-3,Height-3);var color=Enabled?Art.Text:Color.FromArgb(147,123,162);
   using(var p=Art.Round(r,Chrome?7:10*scale)){
    if(!Chrome||over){
     Color a=Primary?Color.FromArgb(192,115,187):Color.FromArgb(85,62,107), b=Primary?Color.FromArgb(151,78,154):Color.FromArgb(62,43,83);
     if(!Enabled){a=Color.FromArgb(57,43,73);b=Color.FromArgb(49,36,65);}
     using(var fill=new LinearGradientBrush(r,a,b,70f))g.FillPath(fill,p);
     if(Enabled&&blend>0)using(var glow=new SolidBrush(Color.FromArgb((int)(blend*27),255,215,251)))g.FillPath(glow,p);
     if(pressed)using(var shade=new SolidBrush(Color.FromArgb(40,20,10,30)))g.FillPath(shade,p);
     if(!Chrome)using(var border=new Pen(Primary?Color.FromArgb(244,185,231):Color.FromArgb(135,110,156),Primary?1.4f:1f))g.DrawPath(border,p);
    }
   }
   float icon=Chrome?22*scale:20*scale;var textSize=TextRenderer.MeasureText(Text,Font);float width=String.IsNullOrEmpty(Text)?icon:textSize.Width+icon+12*scale;float x=(Width-width)/2;
   if(!String.IsNullOrEmpty(Symbol))Art.Icon(g,Symbol,new RectangleF(x,(Height-icon)/2,icon,icon),color);
   if(!String.IsNullOrEmpty(Text))TextRenderer.DrawText(g,Text,Font,new Rectangle((int)(x+icon+10*scale),0,(int)(width-icon),Height),color,TextFormatFlags.VerticalCenter|TextFormatFlags.Left|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
   if(Focused&&ShowFocusCues)ControlPaint.DrawFocusRectangle(g,new Rectangle(5,5,Width-10,Height-10),color,Color.Transparent);
  }
  protected override void Dispose(bool disposing){if(disposing)animation.Dispose();base.Dispose(disposing);}
 }
 public sealed class StatusBadge : Label {
  public StatusBadge(){SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;}
  protected override void OnPaint(PaintEventArgs e){
   var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;bool ready=Text.StartsWith("Ready"),error=Text.StartsWith("Error");Color accent=ready?Color.FromArgb(167,224,171):error?Color.FromArgb(242,153,177):Color.FromArgb(223,174,231);
   using(var path=Art.Round(new RectangleF(1,1,Width-3,Height-3),Height/2))using(var brush=new SolidBrush(Color.FromArgb(69,59,88)))using(var pen=new Pen(Color.FromArgb(150,accent))){g.FillPath(brush,path);g.DrawPath(pen,path);}
   float s=Height/34f;using(var b=new SolidBrush(accent))g.FillEllipse(b,13*s,10*s,14*s,14*s);
   if(ready)using(var p=new Pen(Color.FromArgb(40,65,48),1.5f*s))g.DrawLines(p,new[]{new PointF(16*s,17*s),new PointF(19*s,20*s),new PointF(24*s,14*s)});
   TextRenderer.DrawText(g,Text,Font,new Rectangle((int)(37*s),0,Width-(int)(46*s),Height),Art.Text,TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis);
  }
 }
 public sealed class RecordList : ComboBox {
  public RecordList(){DrawMode=DrawMode.OwnerDrawFixed;DropDownStyle=ComboBoxStyle.DropDownList;FlatStyle=FlatStyle.Flat;BackColor=Color.FromArgb(220,194,232);ForeColor=Color.FromArgb(49,29,65);IntegralHeight=false;MaxDropDownItems=8;}
  protected override void OnFontChanged(EventArgs e){base.OnFontChanged(e);ItemHeight=(int)(Font.Height*1.65);}
  protected override void OnDrawItem(DrawItemEventArgs e){if(e.Index<0)return;bool selected=(e.State&DrawItemState.Selected)!=0;using(var b=new SolidBrush(selected?Color.FromArgb(201,165,222):BackColor))e.Graphics.FillRectangle(b,e.Bounds);var r=e.Bounds;r.X+=12;r.Width-=16;TextRenderer.DrawText(e.Graphics,GetItemText(Items[e.Index]),Font,r,ForeColor,TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);}
  protected override void OnResize(EventArgs e){base.OnResize(e);if(Width>4&&Height>4){var old=Region;using(var path=Art.Round(new RectangleF(0,0,Width,Height),8))Region=new Region(path);if(old!=null)old.Dispose();}}
  protected override void WndProc(ref Message m){base.WndProc(ref m);if(m.Msg==0xF||m.Msg==0x317||m.Msg==0x318){using(var g=m.Msg==0xF?Graphics.FromHwnd(Handle):Graphics.FromHdc(m.WParam)){
   g.SmoothingMode=SmoothingMode.AntiAlias;var r=new RectangleF(1,1,Width-3,Height-3);
   using(var p=Art.Round(r,8))using(var b=new LinearGradientBrush(r,Color.FromArgb(236,212,245),Color.FromArgb(196,161,215),15f))using(var pen=new Pen(Color.FromArgb(243,219,252))){g.FillPath(b,p);g.DrawPath(pen,p);}
   var text=SelectedIndex>=0?GetItemText(SelectedItem):"No saved records";
   TextRenderer.DrawText(g,text,Font,new Rectangle(12,0,Width-48,Height),Enabled?ForeColor:Color.Gray,TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
   using(var pen=new Pen(ForeColor,1.8f)){float x=Width-22,y=Height/2f;g.DrawLines(pen,new[]{new PointF(x-5,y-2),new PointF(x,y+3),new PointF(x+5,y-2)});}
   if(Focused)ControlPaint.DrawFocusRectangle(g,new Rectangle(5,5,Width-36,Height-10),ForeColor,BackColor);
  }}}
 }
 public sealed class RecordStats : Label {
  public RecordStats(){SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;}
  protected override void OnPaint(PaintEventArgs e){var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;float s=Font.Size/10f;
   var values=System.Text.RegularExpressions.Regex.Match(Text,@"Placements: (\d+)\s*\|\s*Upgrades: (\d+)");
   if(!values.Success){base.OnPaint(e);return;}
   Art.Icon(g,"layers",new RectangleF(0,2,20*s,20*s),Art.Muted);Art.Icon(g,"upgrade",new RectangleF(220*s,2,20*s,20*s),Color.FromArgb(229,161,207));
   TextRenderer.DrawText(g,"Placements:",Font,new Rectangle((int)(30*s),0,(int)(110*s),Height),Art.Text,TextFormatFlags.VerticalCenter);
   TextRenderer.DrawText(g,"Upgrades:",Font,new Rectangle((int)(250*s),0,(int)(104*s),Height),Art.Text,TextFormatFlags.VerticalCenter);
   using(var f=new Font(Font,FontStyle.Bold)){TextRenderer.DrawText(g,values.Groups[1].Value,f,new Rectangle((int)(145*s),0,(int)(60*s),Height),ForeColor,TextFormatFlags.VerticalCenter);TextRenderer.DrawText(g,values.Groups[2].Value,f,new Rectangle((int)(356*s),0,(int)(60*s),Height),ForeColor,TextFormatFlags.VerticalCenter);}
   using(var p=new Pen(Color.FromArgb(110,175,141,194)))g.DrawLine(p,201*s,5*s,201*s,Height-5*s);
  }
 }
 public sealed class ShineForm : Form {
  class Entry {public Control C;public Rectangle Bounds;public float FontSize;public FontStyle Style;public string Family;public Font Owned;}
  readonly List<Entry> entries=new List<Entry>();bool arranging;public int DesignHeight=470;
  [DllImport("user32.dll")]static extern bool ReleaseCapture();
  [DllImport("user32.dll")]static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,IntPtr l);
  [DllImport("user32.dll")]public static extern bool SetProcessDPIAware();
  public ShineForm(){AutoScaleMode=AutoScaleMode.None;FormBorderStyle=FormBorderStyle.None;DoubleBuffered=true;BackColor=Color.FromArgb(38,27,57);ForeColor=Art.Text;Text="Starshine";ClientSize=new Size(900,470);MinimumSize=new Size(810,423);StartPosition=FormStartPosition.CenterScreen;MaximizedBounds=Screen.FromPoint(Cursor.Position).WorkingArea;}
  protected override CreateParams CreateParams {get{var p=base.CreateParams;p.ClassStyle|=0x20000;return p;}}
  public void Register(Control c,int x,int y,int w,int h){entries.Add(new Entry{C=c,Bounds=new Rectangle(x,y,w,h),FontSize=c.Font.Size,Style=c.Font.Style,Family=c.Font.FontFamily.Name});Arrange();}
  public void SetDetails(bool visible){float scale=ClientSize.Width/900f;DesignHeight=visible?640:470;if(WindowState==FormWindowState.Normal)ClientSize=new Size(ClientSize.Width,(int)(DesignHeight*scale));Arrange();}
  public void Arrange(){if(arranging||entries==null||WindowState==FormWindowState.Minimized||ClientSize.Width<100||ClientSize.Height<100)return;arranging=true;float sx=ClientSize.Width/900f,sy=ClientSize.Height/(float)DesignHeight,fs=Math.Max(.25f,Math.Min(sx,sy));SuspendLayout();foreach(var e in entries){e.C.SetBounds((int)(e.Bounds.X*sx),(int)(e.Bounds.Y*sy),(int)(e.Bounds.Width*sx),(int)(e.Bounds.Height*sy));if(Math.Abs(e.C.Font.Size-e.FontSize*fs)>.01f){var font=new Font(e.Family,e.FontSize*fs,e.Style);var old=e.Owned;e.C.Font=font;e.Owned=font;if(old!=null)old.Dispose();}}ResumeLayout();arranging=false;Invalidate();}
  protected override void OnResize(EventArgs e){base.OnResize(e);if(Width>3&&Height>3){var old=Region;using(var p=Art.Round(new RectangleF(0,0,Width,Height),WindowState==FormWindowState.Maximized?1:16))Region=new Region(p);if(old!=null)old.Dispose();}Arrange();}
  public void Drag(){ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);}
  protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);if(e.Button==MouseButtons.Left&&e.Y<78*Height/(float)DesignHeight)Drag();}
  protected override void OnMouseDoubleClick(MouseEventArgs e){base.OnMouseDoubleClick(e);if(e.Y<78*Height/(float)DesignHeight)WindowState=WindowState==FormWindowState.Maximized?FormWindowState.Normal:FormWindowState.Maximized;}
  protected override void OnPaintBackground(PaintEventArgs e){
   PaintSurface(e.Graphics);
  }
  public void PaintSurface(Graphics g){
   g.SmoothingMode=SmoothingMode.AntiAlias;
   using(var b=new LinearGradientBrush(ClientRectangle,Color.FromArgb(40,28,57),Color.FromArgb(70,46,91),40f))g.FillRectangle(b,ClientRectangle);
   var state=g.Save();g.ScaleTransform(Width/900f,Height/(float)DesignHeight);
   using(var p=Art.Round(new RectangleF(25,94,850,DesignHeight-118),16))using(var b=new SolidBrush(Color.FromArgb(46,121,91,154)))using(var pen=new Pen(Color.FromArgb(75,196,160,215))){g.FillPath(b,p);g.DrawPath(pen,p);}
   using(var wave=new GraphicsPath()){
    wave.AddBezier(290,78,490,85,440,-16,650,50);wave.AddBezier(650,50,740,82,790,83,900,31);
    using(var pen=new Pen(Color.FromArgb(85,224,153,220),1.2f))g.DrawPath(pen,wave);
    wave.AddBezier(900,31,720,116,555,8,290,78);wave.CloseFigure();using(var b=new SolidBrush(Color.FromArgb(20,225,153,220)))g.FillPath(b,wave);
   }
   using(var pen=new Pen(Color.FromArgb(112,190,148,210)))g.DrawLine(pen,1,78,899,78);
   Art.Icon(g,"record",new RectangleF(46,157,20,20),Art.Muted);Art.Icon(g,"info",new RectangleF(48,280,17,17),Art.Muted);Art.Icon(g,"run",new RectangleF(48,307,17,17),Art.Muted);Art.Star(g,48,39,22,Color.FromArgb(247,199,237));Art.Star(g,817,119,11,Color.FromArgb(179,220,178,230));Art.Star(g,798,132,4,Color.FromArgb(133,220,178,230));Art.Star(g,355,31,4,Color.FromArgb(151,220,178,230));
   using(var pen=new Pen(Color.FromArgb(38,218,159,225),1))g.DrawBezier(pen,5,DesignHeight-42,160,DesignHeight+45,680,DesignHeight-70,895,DesignHeight-12);
   g.Restore(state);using(var p=Art.Round(new RectangleF(1,1,Width-3,Height-3),16))using(var pen=new Pen(Color.FromArgb(189,155,207),1.3f))g.DrawPath(pen,p);
  }
  protected override void Dispose(bool disposing){if(disposing)foreach(var e in entries)if(e.Owned!=null)e.Owned.Dispose();base.Dispose(disposing);}
 }
}

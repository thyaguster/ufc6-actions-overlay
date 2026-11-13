#ifndef UFC6_PHOTO_WATERMARK_RASTER_H
#define UFC6_PHOTO_WATERMARK_RASTER_H
/* Cached grayscale lettering; the GPU supplies glass, alpha and a soft shadow. */
static uint32_t photo_watermark_raster(const PhotoEffects *p,void *out,uint32_t capacity){
 if(!out||capacity<PHOTO_MARK_BYTES||!p->name[0])return 0;
 static wchar_t cached_name[64];static unsigned char cache[PHOTO_MARK_BYTES];static uint32_t width;static int style=-1,font=-1;
 if(wcscmp(cached_name,p->name)||style!=p->mark_style||font!=p->mark_font||!width){
  static const wchar_t *families[]={L"Segoe UI",L"Bahnschrift",L"Georgia",L"Consolas"};
  HDC dc=CreateCompatibleDC(NULL);void *bits=NULL;BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=PHOTO_MARK_W;info.bmiHeader.biHeight=-PHOTO_MARK_H;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  HBITMAP bitmap=dc?CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0):NULL;
  if(!bitmap||!bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return 0;}
  HGDIOBJ old=SelectObject(dc,bitmap);SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,PHOTO_MARK_W/2,PHOTO_MARK_H/2,NULL);SetViewportExtEx(dc,PHOTO_MARK_W,PHOTO_MARK_H,NULL);memset(bits,0,PHOTO_MARK_BYTES);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));SetTextAlign(dc,TA_LEFT|TA_TOP);
  int x=p->mark_style==1?20:p->mark_style==2?18:12,y=p->mark_style==2?23:p->mark_style==3?3:12;
  int size=p->mark_style==2?30:p->mark_style==3?44:34;HFONT lettering=NULL;HGDIOBJ old_font=NULL;SIZE measured={0};
  do{
   if(lettering){SelectObject(dc,old_font);DeleteObject(lettering);}
   lettering=CreateFontW(-size,0,0,0,p->mark_style==3?FW_NORMAL:FW_SEMIBOLD,p->mark_style==3,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,families[p->mark_font]);
   if(!lettering){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return 0;}
   old_font=SelectObject(dc,lettering);GetTextExtentPoint32W(dc,p->name,(int)wcslen(p->name),&measured);size-=2;
  }while(measured.cx>PHOTO_MARK_W/2-x*2&&size>=12);
  TextOutW(dc,x,y,p->name,(int)wcslen(p->name));width=((uint32_t)measured.cx+x*2)*2;if(width>PHOTO_MARK_W)width=PHOTO_MARK_W;
  if(p->mark_style==2){
   HFONT caption=CreateFontW(-13,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
   if(caption){SelectObject(dc,caption);TextOutW(dc,x,5,L"PHOTO BY",8);SelectObject(dc,lettering);DeleteObject(caption);}RECT divider={5,8,7,55};FillRect(dc,&divider,GetStockObject(WHITE_BRUSH));if(width<200)width=200;
  }
  if(p->mark_style==3){RECT line={x,56,(LONG)width/2-x,57};FillRect(dc,&line,GetStockObject(WHITE_BRUSH));}
  GdiFlush();memcpy(cache,bits,PHOTO_MARK_BYTES);lstrcpynW(cached_name,p->name,64);style=p->mark_style;font=p->mark_font;
  SelectObject(dc,old_font);DeleteObject(lettering);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
 }
 memcpy(out,cache,PHOTO_MARK_BYTES);return width;
}
#endif

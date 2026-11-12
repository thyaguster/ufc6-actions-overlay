#ifndef UFC6_PHOTO_HELP_PANEL_H
#define UFC6_PHOTO_HELP_PANEL_H
#define PHOTO_HELP_WIDTH 440
#define PHOTO_HELP_HEIGHT 552
/* Uses the same font selection, theme and vector button artwork as the menu. */
static void photo_help_row(HDC dc,int y,const wchar_t *label,uint32_t a,uint32_t b,const wchar_t *keyboard,const wchar_t *pad_text,int ps){
 RECT name={18,y,168,y+28};text(dc,font_main,label,name,paint_theme.text,DT_LEFT|DT_VCENTER);
 if(a)controller_artwork_draw(dc,170,y+1,26,26,a,ps);
 if(b)controller_artwork_draw(dc,200,y+1,26,26,b,ps);
 if(pad_text){RECT pad={170,y,236,y+28};text(dc,font_small,pad_text,pad,paint_theme.text,DT_LEFT|DT_VCENTER);}
 RECT key={244,y,PHOTO_HELP_WIDTH-16,y+28};text(dc,font_small,keyboard,key,paint_theme.muted,DT_LEFT|DT_VCENTER|DT_END_ELLIPSIS);
}
static void draw_photo_help(HDC dc,RECT r){
 EnterCriticalSection(&lock);MenuState s=menu;OverlayPreferences prefs=preferences;paint_theme=current_theme;frame_font_index=(prefs.font_size-16)/2;wchar_t message[120];message[0]=0;if(GetTickCount64()<toast_until)lstrcpynW(message,status_text,120);LeaveCriticalSection(&lock);
 HBRUSH bg=CreateSolidBrush(paint_theme.background);FillRect(dc,&r,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);
 RECT bar={0,0,4,r.bottom};bg=CreateSolidBrush(paint_theme.accent);FillRect(dc,&bar,bg);DeleteObject(bg);
 RECT heading={18,12,r.right-16,42};text(dc,font_main,L"PHOTO MODE",heading,paint_theme.text,DT_LEFT|DT_VCENTER);
 wchar_t info[100];swprintf(info,100,L"%ls  |  FOV %d%lc  |  Cage %ls",s.photo_frozen?L"Frozen":L"Live",s.photo_fov,0x00b0,s.photo_cage?L"visible":L"native");
 RECT state_line={18,43,r.right-16,71};text(dc,font_small,info,state_line,paint_theme.accent,DT_LEFT|DT_VCENTER);
 RECT hud_line={18,71,r.right-16,99};text(dc,font_small,s.photo_hud?L"HUD hidden  |  Choose clean or overlay screenshots":L"HUD visible  |  Choose clean or overlay screenshots",hud_line,paint_theme.muted,DT_LEFT|DT_VCENTER);
 int ps=s.playstation_icons;
 photo_help_row(dc,110,L"Move camera",2,0,L"I J K L",NULL,ps);
 photo_help_row(dc,140,L"Look around",4,0,L"Arrow keys",NULL,ps);
 photo_help_row(dc,170,L"Up / down",PAD_LB,PAD_RB,L"U / O",NULL,ps);
 photo_help_row(dc,200,L"Zoom",0,0,L"Page Up / Down",ps?L"L2 / R2":L"LT / RT",ps);
 photo_help_row(dc,230,L"Freeze action",PAD_A,0,L"P",NULL,ps);
 wchar_t screenshot[80];binding_key_label(prefs.actions[BIND_SCREENSHOT].key,screenshot,80);
 if(!prefs.actions[BIND_SCREENSHOT].key)lstrcpyW(screenshot,L"Set in Keybinds");
 photo_help_row(dc,260,L"Screenshot",0x8000,0,screenshot,NULL,ps);
 wchar_t overlay_key[80];binding_key_label(prefs.actions[BIND_SCREENSHOT_OVERLAY].key,overlay_key,80);
 photo_help_row(dc,290,L"Overlay shot",PAD_LEFT,0,overlay_key,NULL,ps);
 photo_help_row(dc,320,L"Reset camera",0x1000,0,L"Home",NULL,ps);
 photo_help_row(dc,350,L"Show / hide HUD",PAD_UP,0,L"H",NULL,ps);
 photo_help_row(dc,380,L"Cage visibility",PAD_RIGHT,0,L"C",NULL,ps);
 photo_help_row(dc,410,L"Controls panel",PAD_DOWN,0,L"Tab",NULL,ps);
 photo_help_row(dc,440,L"Exit Photo Mode",PAD_B,0,L"Esc",NULL,ps);
 RECT faster={18,478,r.right-16,506};text(dc,font_small,L"Hold Shift to move faster.  F10 opens tools.",faster,paint_theme.muted,DT_LEFT|DT_VCENTER);
 RECT note={18,511,r.right-16,539};text(dc,font_small,message[0]?message:L"Camera controls stop while the tools menu is open.",note,message[0]?paint_theme.accent:paint_theme.muted,DT_LEFT|DT_VCENTER|DT_END_ELLIPSIS);
}
#endif

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
 RECT heading={18,12,r.right-70,42};text(dc,font_main,s.photo_panel_page==3?L"PHOTO MODE / REPLAY":s.photo_panel_page==1?L"PHOTO MODE / EFFECTS":s.photo_panel_page==2?L"PHOTO MODE / WATERMARK":L"PHOTO MODE",heading,paint_theme.text,DT_LEFT|DT_VCENTER);
 wchar_t page[20];swprintf(page,20,L"%d / %d",s.photo_panel_page+1,(s.replay_ready||s.photo_panel_page==3)?4:3);RECT pager={r.right-66,12,r.right-16,42};text(dc,font_small,page,pager,paint_theme.muted,DT_RIGHT|DT_VCENTER);
 wchar_t info[100];swprintf(info,100,L"%ls  |  FOV %d%lc  |  Cage %ls",s.photo_frozen?L"Frozen":L"Live",s.photo_fov,0x00b0,s.photo_cage?L"visible":L"native");
 RECT state_line={18,43,r.right-16,71};text(dc,font_small,info,state_line,paint_theme.accent,DT_LEFT|DT_VCENTER);
 RECT hud_line={18,71,r.right-16,99};text(dc,font_small,s.screenshot_include_ui?(s.photo_hud?L"HUD hidden  |  Screenshots include overlay":L"HUD visible  |  Screenshots include overlay"):(s.photo_hud?L"HUD hidden  |  Clean screenshots":L"HUD visible  |  Clean screenshots"),hud_line,paint_theme.muted,DT_LEFT|DT_VCENTER);
 int ps=s.playstation_icons,page_key_f8=1,page_key_f4=1;
 for(int i=0;i<BIND_COUNT;++i){unsigned key=prefs.actions[i].key&255;if(key==VK_F8)page_key_f8=0;if(key==VK_F4)page_key_f4=0;}
 const wchar_t *page_key=page_key_f8?L"F8":page_key_f4?L"F4":L"Use controller Menu";
 if(s.photo_panel_page==3){
  for(int i=0;i<5;++i){int y=110+i*38,focused=s.photo_replay_row==i;
   if(focused){RECT row={12,y,r.right-12,y+34};rounded_box(dc,row,7,paint_theme.focus,paint_theme.focus_border);}
   RECT label={18,y,178,y+34},control={174,y,r.right-4,y+34};text(dc,font_main,i==4?L"Speed":i==3?L"Range":i==2?L"Step size":i?L"Position":L"Pause replay",label,paint_theme.text,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
   if(!s.replay_ready){RECT waiting=control;text(dc,font_small,L"Unavailable",waiting,paint_theme.muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
   else if(!i)toggle_switch(dc,control,s.replay_paused,focused);
   else {wchar_t value[64];if(i==4){static const wchar_t *rates[]={L"Native",L"0.1x",L"0.25x",L"0.5x",L"1x",L"2x"};swprintf(value,64,L"%ls",rates[s.replay_speed_index]);}else if(i==3)swprintf(value,64,L"%ls",s.replay_extended?L"Available recording":L"Finish clip");else if(i==2)swprintf(value,64,L"%d ms",menu_replay_step_ms(&s));else swprintf(value,64,L"%.3f / %.3f s",s.replay_position_ms/1000.,s.replay_duration_ms/1000.);value_chip(dc,control,value,focused,1);}
  }
  RECT detail={18,300,r.right-18,347};text(dc,font_small,!s.replay_ready?L"Waiting for replay. Controls resume when it returns.":s.replay_range_failed?L"No larger continuous recording is available here.":L"Range changes pause; Finish clip restores.",detail,paint_theme.muted,DT_LEFT|DT_WORDBREAK);
  photo_help_row(dc,348,L"Select / adjust",PAD_UP,PAD_RIGHT,L"W S / A D",NULL,ps);
  photo_help_row(dc,378,L"Pause / resume",0x1000,0,L"P",NULL,ps);
  photo_help_row(dc,408,L"Step back / next",PAD_LB,PAD_RB,L"Position row",NULL,ps);
  photo_help_row(dc,438,L"Camera page",PAD_B,0,L"Esc exits",NULL,ps);
  wchar_t key[80];binding_key_label(prefs.actions[BIND_SCREENSHOT].key,key,80);photo_help_row(dc,468,L"Screenshot",0x8000,0,key,NULL,ps);
  photo_help_row(dc,498,L"Next page",PAD_MENU,0,page_key,NULL,ps);
  return;
 }
 if(s.photo_panel_page){
  int mark=s.photo_panel_page==2,count=mark?MARK_BACK:FX_BACK,selected=mark?s.photo_mark_row:s.photo_effect_row;
  for(int i=0;i<count;++i){int y=110+i*30,focused=selected==i;wchar_t value[64];
   if(focused){RECT row={12,y,r.right-12,y+28};rounded_box(dc,row,7,paint_theme.focus,paint_theme.focus_border);}
   RECT label={18,y,214,y+28};text(dc,font_main,mark?photo_mark_label(i):photo_effect_label(i),label,paint_theme.text,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   RECT control={210,y,r.right-4,y+28};
   if(mark&&i==MARK_ENABLED)toggle_switch(dc,control,s.effects.watermark&&s.effects.name[0],focused);
   else if(mark)value_chip(dc,control,photo_mark_value(&s.effects,i,value,64),focused,i!=MARK_NAME);
   else if(i<FX_RESET){photo_effect_value(&s.effects,i,value,64);value_chip(dc,control,value,focused,1);}
  }
  if(mark){RECT help={18,365,r.right-18,412};text(dc,font_small,L"Automatic PORTHEON player name.\nStyle, font and color preview live.",help,paint_theme.muted,DT_LEFT|DT_WORDBREAK);}
  photo_help_row(dc,420,L"Camera page",PAD_B,0,L"Esc exits",NULL,ps);
  wchar_t key[80];binding_key_label(prefs.actions[BIND_SCREENSHOT].key,key,80);photo_help_row(dc,450,L"Screenshot",0x8000,0,key,NULL,ps);
  photo_help_row(dc,480,L"Next page",PAD_MENU,0,page_key,NULL,ps);
  RECT tip={18,511,r.right-16,539};text(dc,font_small,message[0]?message:L"D-pad: select / adjust. Keyboard: W S / A D.",tip,message[0]?paint_theme.accent:paint_theme.muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);return;
 }

 photo_help_row(dc,110,L"Move camera",2,0,L"I J K L",NULL,ps);
 photo_help_row(dc,140,L"Look around",4,0,L"Arrow keys",NULL,ps);
 photo_help_row(dc,170,L"Up / down",PAD_LB,PAD_RB,L"U / O",NULL,ps);
 photo_help_row(dc,200,L"Zoom",0,0,L"Page Up / Down",ps?L"L2 / R2":L"LT / RT",ps);
 photo_help_row(dc,230,L"Freeze action",PAD_A,0,L"P",NULL,ps);
 wchar_t screenshot[80];binding_key_label(prefs.actions[BIND_SCREENSHOT].key,screenshot,80);
 if(!prefs.actions[BIND_SCREENSHOT].key)lstrcpyW(screenshot,L"Set in Keybinds");
 photo_help_row(dc,260,L"Screenshot",0x8000,0,screenshot,NULL,ps);
 photo_help_row(dc,290,L"Reset camera",0x1000,0,L"Home",NULL,ps);
 photo_help_row(dc,320,L"Show / hide HUD",PAD_UP,0,L"H",NULL,ps);
 photo_help_row(dc,350,L"Cage visibility",PAD_RIGHT,0,L"C",NULL,ps);
 photo_help_row(dc,380,L"Controls panel",PAD_DOWN,0,L"Tab",NULL,ps);
 photo_help_row(dc,410,L"Exit Photo Mode",PAD_B,0,L"Esc",NULL,ps);
 photo_help_row(dc,440,L"Effects page",PAD_MENU,0,page_key,NULL,ps);
 RECT note={18,511,r.right-16,539};text(dc,font_small,message[0]?message:L"Hold Shift to move faster. F10 opens tools.",note,message[0]?paint_theme.accent:paint_theme.muted,DT_LEFT|DT_VCENTER|DT_END_ELLIPSIS);
}
#endif

#ifndef UFC6_PHOTO_PANEL_CONTROLS_H
#define UFC6_PHOTO_PANEL_CONTROLS_H
/* Edge-triggered controls. Camera sticks/zoom remain independent on editor pages. */
enum { PHOTO_PANEL_EFFECTS_CHANGED=1,PHOTO_PANEL_REPLAY_TOGGLE,PHOTO_PANEL_REPLAY_BACK,PHOTO_PANEL_REPLAY_FORWARD,PHOTO_PANEL_REPLAY_STEP_CHANGED,PHOTO_PANEL_REPLAY_RANGE,PHOTO_PANEL_REPLAY_SPEED };
enum { PHOTO_PANEL_PAGE_KEY=64,PHOTO_PANEL_UP_KEY=128,PHOTO_PANEL_DOWN_KEY=256,PHOTO_PANEL_LEFT_KEY=512,PHOTO_PANEL_RIGHT_KEY=1024 };
static int photo_panel_editing(const MenuState *s){return !s->photo_help_hidden&&s->photo_panel_page>0;}
static int photo_panel_controls(MenuState *s,uint32_t *pad,unsigned keys){
 if((*pad&PAD_MENU)||(keys&PHOTO_PANEL_PAGE_KEY)){s->photo_panel_page=s->photo_help_hidden?1:(s->photo_panel_page+1)%(s->replay_ready?4:3);s->photo_help_hidden=0;*pad=0;return 0;}
 if(!photo_panel_editing(s))return 0;
 if(*pad&PAD_B){s->photo_panel_page=0;*pad&=~PAD_B;return 0;}
 if(s->photo_panel_page==3){
  if(!s->replay_ready){*pad&=~(PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT|PAD_A|PAD_LB|PAD_RB|0x1000);return 0;}
  if(*pad&0x1000){*pad&=~0x1000;return PHOTO_PANEL_REPLAY_TOGGLE;}
  if(*pad&(PAD_LB|PAD_RB)){int back=!!(*pad&PAD_LB),forward=!!(*pad&PAD_RB);*pad&=~(PAD_LB|PAD_RB);return back==forward?0:back?PHOTO_PANEL_REPLAY_BACK:PHOTO_PANEL_REPLAY_FORWARD;}

  if((*pad&(PAD_UP|PAD_DOWN))||(keys&(PHOTO_PANEL_UP_KEY|PHOTO_PANEL_DOWN_KEY)))s->photo_replay_row=(s->photo_replay_row+(((*pad&PAD_UP)||(keys&PHOTO_PANEL_UP_KEY))?4:1))%5;
  int left=!!((*pad&PAD_LEFT)||(keys&PHOTO_PANEL_LEFT_KEY)),right=!!((*pad&(PAD_RIGHT|PAD_A))||(keys&PHOTO_PANEL_RIGHT_KEY));
  *pad&=~(PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT|PAD_A);
  if(left==right)return 0;
  if(s->photo_replay_row==4){s->replay_speed_index=(s->replay_speed_index+(left?5:1))%6;return PHOTO_PANEL_REPLAY_SPEED;}
  if(s->photo_replay_row==3)return PHOTO_PANEL_REPLAY_RANGE;
  if(s->photo_replay_row==2){s->replay_step_index=(s->replay_step_index+(left?6:1))%7;return PHOTO_PANEL_REPLAY_STEP_CHANGED;}
  return s->photo_replay_row?(left?PHOTO_PANEL_REPLAY_BACK:PHOTO_PANEL_REPLAY_FORWARD):PHOTO_PANEL_REPLAY_TOGGLE;
 }
 int *row=s->photo_panel_page==1?&s->photo_effect_row:&s->photo_mark_row,count=s->photo_panel_page==1?FX_BACK:MARK_BACK;
 if((*pad&PAD_UP)||(keys&PHOTO_PANEL_UP_KEY))*row=(*row+count-1)%count;
 if((*pad&PAD_DOWN)||(keys&PHOTO_PANEL_DOWN_KEY))*row=(*row+1)%count;
 int left=!!((*pad&PAD_LEFT)||(keys&PHOTO_PANEL_LEFT_KEY)),right=!!((*pad&(PAD_RIGHT|PAD_A))||(keys&PHOTO_PANEL_RIGHT_KEY));
 if(left==right)return 0;
 if(s->photo_panel_page==1)photo_effects_change(&s->effects,*row,left?-1:1);
 else {if(*row==MARK_NAME)return 0;photo_watermark_change(&s->effects,*row,left?-1:1);}
 return 1;
}
#endif

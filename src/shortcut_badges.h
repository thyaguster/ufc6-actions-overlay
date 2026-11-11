/* Vector controller glyphs used by every shortcut badge. */
static int shortcut_button_width(uint32_t bit){(void)bit;return 32;}
static void shortcut_glyph(HDC dc,int x,int y,uint32_t bit,int ps){controller_artwork_draw(dc,x,y,32,24,bit,ps);}
static void shortcut_keycap(HDC dc,int x,int y,int width,const wchar_t *label){
    RECT cap={x,y+2,x+width,y+22};rounded_box(dc,cap,5,paint_theme.background,paint_theme.border);
    RECT text_box={x+3,y,x+width-3,y+24};text(dc,font_small,label,text_box,paint_theme.text,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
static void binding_chip(HDC dc,RECT row,const ActionBinding *binding,int selected,int ps,int recovery,const wchar_t *extra){
    static const uint32_t bits[]={2,4,PAD_LB,PAD_RB,PAD_MENU,PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,0x1000,PAD_B,PAD_A,0x8000};
    if(!binding->key&&!binding->pad&&!recovery){value_chip(dc,row,L"Not assigned",selected,0);return;}
    wchar_t key[100]=L"";if(binding->key)binding_key_label(binding->key,key,100);
    HFONT old=SelectObject(dc,render_font(font_small));SIZE k={0},e={0};if(key[0])GetTextExtentPoint32W(dc,key,(int)wcslen(key),&k);if(extra)GetTextExtentPoint32W(dc,extra,(int)wcslen(extra),&e);SelectObject(dc,old);
    int n=(int)menu_button_count(binding->pad),pad_width=n?32*n+10*(n-1):0;
    int fixed=16+pad_width+(recovery?34:0)+(extra?e.cx+8:0),pieces=(pad_width>0)+(recovery!=0)+(key[0]!=0)+(extra!=NULL);
    fixed+=pieces>1?10*(pieces-1):0;int key_width=key[0]?k.cx+12:0;if(key_width+fixed>246)key_width=246-fixed;if(key_width<28&&key[0])key_width=28;
    int width=fixed+key_width,y=(row.top+row.bottom)/2-12,x=row.right-14-width;RECT box={x,y,x+width,y+24};rounded_box(dc,box,8,selected?paint_theme.focus:paint_theme.surface,selected?paint_theme.focus_border:paint_theme.border);x+=8;
    if(recovery){shortcut_keycap(dc,x,y,34,L"F10");x+=34;if(key[0]||pad_width||extra)x+=10;}
    if(key[0]){shortcut_keycap(dc,x,y,key_width,key);x+=key_width;if(pad_width||extra)x+=10;}
    int drawn=0;for(unsigned i=0;i<sizeof(bits)/sizeof(bits[0]);++i)if(binding->pad&bits[i]){if(drawn){RECT plus={x,y,x+10,y+24};text(dc,font_small,L"+",plus,paint_theme.muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);x+=10;}shortcut_glyph(dc,x,y,bits[i],ps);x+=shortcut_button_width(bits[i]);++drawn;}
    if(extra){if(pad_width)x+=10;RECT label={x,y,row.right-20,y+24};text(dc,font_small,extra,label,paint_theme.muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
}

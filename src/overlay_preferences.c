#define WIN32_LEAN_AND_MEAN
#include "overlay_preferences.h"
#include <wchar.h>
#include <stdio.h>
#include <string.h>
const wchar_t *binding_names[BIND_COUNT]={L"Open overlay",L"Take screenshot",L"Cheats",L"Settings",L"Savegame backup",L"Restart game",L"Quit game",L"Restart PC",L"Shut down PC",L"Photo Mode",L"Screenshot + overlay"};
static const wchar_t *binding_ids[BIND_COUNT]={L"Open",L"Screenshot",L"Cheats",L"Settings",L"Backup",L"RestartGame",L"QuitGame",L"RestartPC",L"ShutdownPC",L"PhotoMode",L"ScreenshotOverlay"};
const wchar_t *theme_choices[7]={L"From config",L"Green",L"Blue",L"Red",L"Purple",L"Amber",L"Back"};
int binding_key_valid(uint32_t key){
    unsigned vk=key&255;if(!key)return 1;
    if(key&~0x7ffu||vk<8||vk==VK_ESCAPE||vk==VK_F10||vk==VK_LWIN||vk==VK_RWIN||vk==VK_APPS)return 0;
    return vk!=VK_SHIFT&&vk!=VK_CONTROL&&vk!=VK_MENU&&!(vk>=VK_LSHIFT&&vk<=VK_RMENU);
}
static int pad_valid(uint32_t pad){unsigned n=0;for(uint32_t b=pad;b;b&=b-1)++n;return !pad||(!(pad&~0xfcfeu)&&n>=2&&n<=4);}
int binding_set(OverlayPreferences *p,int action,int keyboard,uint32_t value){
    if(action<0||action>=BIND_COUNT||(keyboard?!binding_key_valid(value):!pad_valid(value))||(!keyboard&&action==BIND_OPEN&&!value))return 0;
    for(int i=0;i<BIND_COUNT;++i)if(i!=action&&value){uint32_t other=keyboard?p->actions[i].key:p->actions[i].pad;
        if(other&&(keyboard?other==value:((other&value)==other||(other&value)==value)))return 0;}
    if(keyboard)p->actions[action].key=value;else p->actions[action].pad=value;return 1;
}
int binding_poll_pad(BindingLatch *l,uint32_t chord,uint32_t buttons,uint64_t now,int enabled){
    if(!chord||!enabled){l->since=0;l->latched=buttons!=0;return 0;}
    if((buttons&chord)!=chord){l->since=0;if(!buttons)l->latched=0;return 0;}
    if(l->latched)return 0;
    if(!l->since)l->since=now?now:1;
    if(now-l->since<800)return 0;l->latched=1;return 1;
}
int binding_poll_key(BindingLatch *l,uint32_t key,const unsigned char down[256],int enabled){
    unsigned mods=(down[VK_CONTROL]?KEY_CTRL:0)|(down[VK_MENU]?KEY_ALT:0)|(down[VK_SHIFT]?KEY_SHIFT:0);
    int held=key&&down[key&255]&&mods==(key&0x700);int fire=enabled&&held&&!l->latched;l->latched=held;return fire;
}
void binding_key_label(uint32_t key,wchar_t *out,size_t count){
    if(!count)return;out[0]=0;if(!key){lstrcpynW(out,L"Not assigned",(int)count);return;}
    wchar_t name[80]=L"",full[128];UINT vk=key&255,scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC);LONG code=(LONG)(scan<<16);
    if(vk==VK_LEFT||vk==VK_RIGHT||vk==VK_UP||vk==VK_DOWN||vk==VK_INSERT||vk==VK_DELETE||vk==VK_HOME||vk==VK_END||vk==VK_PRIOR||vk==VK_NEXT||vk==VK_DIVIDE)code|=1<<24;
    if(!GetKeyNameTextW(code,name,80))swprintf(name,80,L"Key %u",vk);
    swprintf(full,128,L"%ls%ls%ls%ls",key&KEY_CTRL?L"Ctrl + ":L"",key&KEY_ALT?L"Alt + ":L"",key&KEY_SHIFT?L"Shift + ":L"",name);lstrcpynW(out,full,(int)count);
}
void preferences_seed_appearance(const wchar_t *ini){
    wchar_t cfg[MAX_PATH],value[32],existing[64];lstrcpynW(cfg,ini,MAX_PATH);wchar_t *slash=wcsrchr(cfg,L'\\');if(slash)lstrcpyW(slash+1,L"ufc6-overlay-theme.cfg");else return;
    int font=(int)GetPrivateProfileIntW(L"Appearance",L"FontSize",18,cfg);if(font!=16&&font!=18&&font!=20)font=18;
    int scale=(int)GetPrivateProfileIntW(L"Appearance",L"UIScale",100,cfg);if(scale<50||scale>300||scale%25)scale=100;
    const wchar_t *styles[]={L"Rounded",L"Square",L"Outline",L"Pill",L"Octagon"};int style=0;GetPrivateProfileStringW(L"Appearance",L"BoxStyle",L"Rounded",value,32,cfg);
    for(int i=0;i<5;++i)if(!lstrcmpiW(value,styles[i]))style=i;
    int window=0;GetPrivateProfileStringW(L"Appearance",L"WindowSize",L"Compact",value,32,cfg);if(!lstrcmpiW(value,L"Expanded"))window=1;else if(!lstrcmpiW(value,L"Tall"))window=2;
    const wchar_t *keys[]={L"FontSize",L"UIScale",L"BoxStyle",L"WindowSize"};int values[]={font,scale,style,window};
    for(int i=0;i<4;++i){GetPrivateProfileStringW(L"Menu",keys[i],L"missing",existing,64,ini);if(!lstrcmpW(existing,L"missing")){swprintf(value,32,L"%d",values[i]);WritePrivateProfileStringW(L"Menu",keys[i],value,ini);}}
}
void preferences_load(OverlayPreferences *p,const wchar_t *ini,uint32_t open_pad){
    memset(p,0,sizeof(*p));p->actions[0].pad=open_pad;
    p->font_size=(int)GetPrivateProfileIntW(L"Menu",L"FontSize",18,ini);if(p->font_size!=16&&p->font_size!=18&&p->font_size!=20)p->font_size=18;
    p->box_style=(int)GetPrivateProfileIntW(L"Menu",L"BoxStyle",0,ini);if(p->box_style<0||p->box_style>4)p->box_style=0;
    p->theme_choice=(int)GetPrivateProfileIntW(L"Menu",L"ThemeChoice",0,ini);if(p->theme_choice<0||p->theme_choice>5)p->theme_choice=0;
    p->window_size=(int)GetPrivateProfileIntW(L"Menu",L"WindowSize",0,ini);if(p->window_size<0||p->window_size>2)p->window_size=0;
    for(int i=0;i<BIND_COUNT;++i){wchar_t name[64],value[64],*end;
        swprintf(name,64,L"%lsKey",binding_ids[i]);GetPrivateProfileStringW(L"Keybinds",name,i==BIND_PHOTO?L"0x78":i==BIND_SCREENSHOT_OVERLAY?L"0x57b":L"0",value,64,ini);unsigned long v=wcstoul(value,&end,0);if(!*end)binding_set(p,i,1,(uint32_t)v);
        if(!i)continue;swprintf(name,64,L"%lsPad",binding_ids[i]);GetPrivateProfileStringW(L"Keybinds",name,L"0",value,64,ini);v=wcstoul(value,&end,0);if(!*end)binding_set(p,i,0,(uint32_t)v);
    }
}
int preferences_save(const OverlayPreferences *p,const wchar_t *ini){
    int ok=1;wchar_t name[64],value[32];
    for(int i=0;i<BIND_COUNT;++i){swprintf(name,64,L"%lsKey",binding_ids[i]);swprintf(value,32,L"0x%08x",p->actions[i].key);ok=WritePrivateProfileStringW(L"Keybinds",name,value,ini)&&ok;
        if(!i)continue;swprintf(name,64,L"%lsPad",binding_ids[i]);swprintf(value,32,L"0x%08x",p->actions[i].pad);ok=WritePrivateProfileStringW(L"Keybinds",name,value,ini)&&ok;}
    swprintf(value,32,L"%d",p->font_size);ok=WritePrivateProfileStringW(L"Menu",L"FontSize",value,ini)&&ok;
    swprintf(value,32,L"%d",p->box_style);ok=WritePrivateProfileStringW(L"Menu",L"BoxStyle",value,ini)&&ok;
    swprintf(value,32,L"%d",p->window_size);ok=WritePrivateProfileStringW(L"Menu",L"WindowSize",value,ini)&&ok;
    swprintf(value,32,L"%d",p->theme_choice);return WritePrivateProfileStringW(L"Menu",L"ThemeChoice",value,ini)&&ok;
}
static COLORREF blend(COLORREF a,COLORREF b,unsigned weight){return RGB((GetRValue(a)*weight+GetRValue(b)*(100-weight))/100,(GetGValue(a)*weight+GetGValue(b)*(100-weight))/100,(GetBValue(a)*weight+GetBValue(b)*(100-weight))/100);}
static COLORREF color(const wchar_t *cfg,const wchar_t *key,COLORREF fallback){wchar_t value[32],*end;GetPrivateProfileStringW(L"Theme",key,L"",value,32,cfg);if(value[0]!=L'#'||wcslen(value)!=7)return fallback;for(int i=1;i<7;++i)if(!((value[i]>=L'0'&&value[i]<=L'9')||(value[i]>=L'a'&&value[i]<=L'f')||(value[i]>=L'A'&&value[i]<=L'F')))return fallback;unsigned long v=wcstoul(value+1,&end,16);return *end?fallback:RGB((v>>16)&255,(v>>8)&255,v&255);}
void theme_load(OverlayTheme *t,const wchar_t *cfg,int choice){
    wchar_t preset[32];GetPrivateProfileStringW(L"Theme",L"Preset",L"Green",preset,32,cfg);
    int selected=choice;const COLORREF accents[]={RGB(118,224,148),RGB(102,177,255),RGB(244,104,117),RGB(188,141,255),RGB(245,190,88)};
    if(!selected){selected=1;for(int i=1;i<=5;++i)if(!lstrcmpiW(preset,theme_choices[i]))selected=i;}
    if(selected<1||selected>5)selected=1;
    *t=(OverlayTheme){RGB(17,22,30),RGB(29,37,49),RGB(42,53,68),RGB(218,228,240),RGB(176,184,197),accents[selected-1],0,0,L""};
    lstrcpynW(t->name,choice?theme_choices[selected]:preset,32);
    if(!choice){t->accent=color(cfg,L"Accent",t->accent);t->background=color(cfg,L"Background",t->background);t->surface=color(cfg,L"Surface",t->surface);t->border=color(cfg,L"Border",t->border);t->text=color(cfg,L"Text",t->text);t->muted=color(cfg,L"Muted",t->muted);}
    t->focus=blend(t->accent,t->surface,13);t->focus_border=blend(t->accent,t->border,65);
}
void theme_create_default(const wchar_t *cfg){
    HANDLE f=CreateFileW(cfg,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return;
    const char body[]="; UFC 6 Actions Overlay launcher theme. Existing files are never overwritten.\r\n; Presets: Green, Blue, Red, Purple, Amber. Optional overrides: #RRGGBB.\r\n; Settings > Appearance > Theme > From config follows this file (reloaded while running).\r\n[Theme]\r\nPreset=Green\r\n;Accent=#76E094\r\n;Background=#11161E\r\n;Surface=#1D2531\r\n;Border=#2A3544\r\n;Text=#DAE4F0\r\n;Muted=#B0B8C5\r\n\r\n; First-run defaults only; saved client settings take priority.\r\n[Appearance]\r\nUIScale=100\r\nFontSize=18\r\nBoxStyle=Rounded\r\nWindowSize=Compact\r\n";
    DWORD wrote=0;WriteFile(f,body,sizeof(body)-1,&wrote,NULL);CloseHandle(f);
}

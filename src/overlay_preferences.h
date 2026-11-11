#ifndef UFC6_OVERLAY_PREFERENCES_H
#define UFC6_OVERLAY_PREFERENCES_H
#include <windows.h>
#include <stdint.h>
enum { BIND_OPEN,BIND_SCREENSHOT,BIND_CHEATS,BIND_SETTINGS,BIND_BACKUP,BIND_RESTART,BIND_QUIT,BIND_REBOOT,BIND_SHUTDOWN,BIND_COUNT };
enum { KEY_CTRL=0x100,KEY_ALT=0x200,KEY_SHIFT=0x400 };
typedef struct {uint32_t key,pad;} ActionBinding;
typedef struct {ActionBinding actions[BIND_COUNT];int theme_choice,font_size,box_style,window_size;} OverlayPreferences;
typedef struct {uint64_t since;int latched;} BindingLatch;
typedef struct {COLORREF background,surface,border,text,muted,accent,focus,focus_border;wchar_t name[32];} OverlayTheme;
extern const wchar_t *binding_names[BIND_COUNT];
extern const wchar_t *theme_choices[7];
int binding_set(OverlayPreferences *p,int action,int keyboard,uint32_t value);
int binding_key_valid(uint32_t key);
int binding_poll_pad(BindingLatch *l,uint32_t chord,uint32_t buttons,uint64_t now,int enabled);
int binding_poll_key(BindingLatch *l,uint32_t key,const unsigned char down[256],int enabled);
void binding_key_label(uint32_t key,wchar_t *out,size_t count);
void preferences_seed_appearance(const wchar_t *ini);
void preferences_load(OverlayPreferences *p,const wchar_t *ini,uint32_t open_pad);
int preferences_save(const OverlayPreferences *p,const wchar_t *ini);
void theme_load(OverlayTheme *out,const wchar_t *cfg,int choice);
void theme_create_default(const wchar_t *cfg);
#endif

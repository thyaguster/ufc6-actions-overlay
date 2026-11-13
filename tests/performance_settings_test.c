#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "performance_settings.h"
#include "launch_config.h"
#pragma clang diagnostic ignored "-Wunused-function"
#include "menu_state.h"
static wchar_t root[MAX_PATH],bin[MAX_PATH],ini[MAX_PATH],launcher[MAX_PATH],overlay_ini[MAX_PATH];
static void write(const wchar_t *key,const wchar_t *value){assert(WritePrivateProfileStringW(L"UFC6",key,value,ini));}
int main(void){
 wchar_t temp[MAX_PATH],self[MAX_PATH];GetTempPathW(MAX_PATH,temp);GetTempFileNameW(temp,L"pfm",0,root);DeleteFileW(root);assert(CreateDirectoryW(root,NULL));swprintf(bin,MAX_PATH,L"%ls\\bin",root);assert(CreateDirectoryW(bin,NULL));swprintf(ini,MAX_PATH,L"%ls\\UFC6.ini",root);swprintf(launcher,MAX_PATH,L"%ls\\UFC6.exe",root);swprintf(overlay_ini,MAX_PATH,L"%ls\\ufc6-quit-menu.ini",bin);GetModuleFileNameW(NULL,self,MAX_PATH);assert(CopyFileW(self,launcher,TRUE));
 PerformanceSettings original,desired,loaded;assert(performance_load(bin,&original)==PERF_UNAVAILABLE);
 HANDLE file=CreateFileW(ini,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);assert(file!=INVALID_HANDLE_VALUE);WORD bom=0xfeff;DWORD done;assert(WriteFile(file,&bom,2,&done,NULL));CloseHandle(file);
 write(L"game",L"E:\\Game data \x03a9");write(L"user",L"Photo player");write(L"parsec",L"1");write(L"quality",L"1");write(L"render_scale_75",L"1");write(L"crowd_off",L"1");write(L"dof_off",L"0");write(L"drs_off",L"1");write(L"render_scale",L"133");assert(WritePrivateProfileStringW(L"Other",L"Keep",L"untouched",ini));
 assert(performance_load(bin,&original)==PERF_OK&&original.crowd_off&&!original.particles_off&&!original.dof_off&&original.render_scale==133);
 desired=original;performance_adjust(&desired,PERF_PARTICLES,1);performance_adjust(&desired,PERF_DOF,1);assert(performance_save(bin,&original,&desired)==PERF_OK);assert(performance_load(bin,&loaded)==PERF_OK&&performance_equal(&loaded,&desired));assert(GetPrivateProfileIntW(L"UFC6",L"parsec",0,ini)==1&&GetPrivateProfileIntW(L"UFC6",L"quality",0,ini)==1&&GetPrivateProfileIntW(L"UFC6",L"render_scale_75",0,ini)==1);
 wchar_t value[100];GetPrivateProfileStringW(L"Other",L"Keep",L"",value,100,ini);assert(!wcscmp(value,L"untouched"));GetPrivateProfileStringW(L"UFC6",L"user",L"",value,100,ini);assert(!wcscmp(value,L"Photo player"));GetPrivateProfileStringW(L"UFC6",L"game",L"",value,100,ini);assert(!wcscmp(value,L"E:\\Game data \x03a9"));
 BY_HANDLE_FILE_INFORMATION stale;file=CreateFileW(ini,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(file,&stale));CloseHandle(file);assert(WritePrivateProfileStringW(L"Other",L"New",L"concurrent update",ini));const wchar_t *one_key[]={L"render_scale"},*one_value[]={L"200"};assert(!launch_config_update_values(ini,L"UFC6",one_key,one_value,1,&stale)&&GetLastError()==ERROR_RETRY);assert(GetPrivateProfileIntW(L"UFC6",L"render_scale",0,ini)==133);
 write(L"crowd_off",L"0");assert(performance_save(bin,&loaded,&desired)==PERF_CHANGED_EXTERNALLY);assert(GetPrivateProfileIntW(L"UFC6",L"crowd_off",1,ini)==0);assert(performance_load(bin,&loaded)==PERF_OK);
 DWORD old=GetFileAttributesW(ini);assert(SetFileAttributesW(ini,FILE_ATTRIBUTE_READONLY));assert(performance_save(bin,&loaded,&desired)==PERF_WRITE_FAILED);assert(SetFileAttributesW(ini,old));
 file=CreateFileW(ini,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);assert(file!=INVALID_HANDLE_VALUE);assert(performance_save(bin,&loaded,&desired)==PERF_WRITE_FAILED);CloseHandle(file);
 LaunchConfig custom;launch_config_default(bin,&custom);custom.custom=1;wcscpy(custom.executable,self);assert(launch_config_save(overlay_ini,&custom));assert(performance_load(bin,&loaded)==PERF_UNAVAILABLE);DeleteFileW(overlay_ini);
 assert(performance_load(bin,&loaded)==PERF_OK);desired=loaded;performance_adjust(&desired,PERF_DYNAMIC,1);assert(!desired.drs_off&&!desired.render_scale);performance_adjust(&desired,PERF_SCALE,1);assert(desired.drs_off&&desired.render_scale==33);
 write(L"render_scale",L"0");write(L"render_scale_75",L"1");assert(performance_load(bin,&loaded)==PERF_OK);desired=loaded;desired.crowd_off=!desired.crowd_off;assert(performance_save(bin,&loaded,&desired)==PERF_OK&&GetPrivateProfileIntW(L"UFC6",L"render_scale_75",0,ini)==1);
 assert(performance_load(bin,&loaded)==PERF_OK);desired=loaded;desired.render_scale=67;assert(performance_save(bin,&loaded,&desired)==PERF_OK&&GetPrivateProfileIntW(L"UFC6",L"render_scale_75",1,ini)==0);
 write(L"dof_off",L"9");assert(performance_load(bin,&loaded)==PERF_UNAVAILABLE);write(L"dof_off",L"0");
 MenuState menu={.visible=1,.armed=1,.page=PAGE_SETTINGS_HOME,.selection=4};assert(menu_button(&menu,PAD_A,1)==MENU_PERFORMANCE_OPEN&&menu.page==PAGE_PERFORMANCE);menu.performance_available=1;menu.performance=(PerformanceSettings){.drs_off=1,.render_scale=133};menu.performance_original=menu.performance;menu_tick(&menu,0,1,1);assert(menu_button(&menu,PAD_A,1)==MENU_CHANGED&&menu.performance_dirty&&menu.performance.crowd_off);menu_tick(&menu,0,2,1);assert(menu_button(&menu,PAD_B,1)==MENU_CHANGED&&menu.page==PAGE_PERFORMANCE_DISCARD&&!menu.selection);menu_tick(&menu,0,3,1);menu_button(&menu,PAD_A,1);assert(menu.page==PAGE_PERFORMANCE&&menu.performance_dirty);
 menu.selection=PERF_RESTART;menu_tick(&menu,0,4,1);menu_button(&menu,PAD_A,1);assert(menu.page==PAGE_PERFORMANCE_RESTART&&!menu.selection);menu_tick(&menu,0,5,1);menu_button(&menu,PAD_B,1);assert(menu.page==PAGE_PERFORMANCE&&menu.selection==PERF_RESTART);
 assert(DeleteFileW(ini));assert(DeleteFileW(launcher));assert(RemoveDirectoryW(bin));assert(RemoveDirectoryW(root));
 puts("PASS: PORTHEON settings detection, atomic Unicode save, unrelated settings/legacy scale preservation, external changes and write conflicts, custom-launcher rejection, scale/dynamic interaction, pending edits and Cancel-first restart/discard pages.");return 0;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "../src/overlay_bootstrap.h"
#undef NDEBUG
#include <assert.h>
static void clear(void){SetEnvironmentVariableW(L"VK_LAYER_PATH",NULL);SetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",NULL);SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",NULL);SetEnvironmentVariableW(L"VK_LOADER_DEBUG",NULL);}
int main(void){
 wchar_t directory[MAX_PATH],path[MAX_PATH],value[16384],again[16384];assert(GetModuleFileNameW(NULL,directory,MAX_PATH));wchar_t *slash=wcsrchr(directory,92);assert(slash);*slash=0;slash=wcsrchr(directory,92);assert(slash&&!wcscmp(slash+1,L"bootstrap-compat"));
 const wchar_t *names[]={L"version.dll",L"ufc6_menu_layer.dll",L"ufc6_menu_layer.json"};
 for(int i=0;i<3;++i){swprintf(path,MAX_PATH,L"%ls\\%ls",directory,names[i]);HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);assert(file!=INVALID_HANDLE_VALUE);CloseHandle(file);}
 clear();overlay_bootstrap(GetModuleHandleW(NULL));assert(GetEnvironmentVariableW(L"VK_LAYER_PATH",value,16384)&&!wcscmp(value,directory));assert(GetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",value,16384)&&!wcscmp(value,directory));assert(GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",value,16384)&&!wcscmp(value,L"VK_LAYER_UFC6_quit_menu"));
 clear();SetEnvironmentVariableW(L"VK_LAYER_PATH",L"C:\\Existing One;C:\\Existing Two");SetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",L"C:\\Existing Two;D:\\Extra");SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",L"VK_LAYER_other");SetEnvironmentVariableW(L"VK_LOADER_DEBUG",L"all");overlay_bootstrap(GetModuleHandleW(NULL));assert(GetEnvironmentVariableW(L"VK_LAYER_PATH",value,16384));assert(wcsstr(value,L"Existing One")&&wcsstr(value,L"Existing Two")&&wcsstr(value,L"Extra"));wcscpy(again,value);overlay_bootstrap(GetModuleHandleW(NULL));assert(GetEnvironmentVariableW(L"VK_LAYER_PATH",value,16384)&&!wcscmp(value,again));assert(GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",value,16384)&&!wcscmp(value,L"VK_LAYER_UFC6_quit_menu;VK_LAYER_other"));assert(GetEnvironmentVariableW(L"VK_LOADER_DEBUG",value,16384)&&!wcscmp(value,L"all"));
 wchar_t small[8]=L"abc";assert(!overlay_path_merge(small,8,L"too-long-path")&&!wcscmp(small,L"abc"));
 swprintf(path,MAX_PATH,L"%ls\\version.dll",directory);assert(DeleteFileW(path));clear();overlay_bootstrap(GetModuleHandleW(NULL));assert(!GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",NULL,0));assert(!GetEnvironmentVariableW(L"VK_LAYER_PATH",NULL,0));
 for(int i=1;i<3;++i){swprintf(path,MAX_PATH,L"%ls\\%ls",directory,names[i]);assert(DeleteFileW(path));}
 puts("PASS: legacy-only discovery, inherited primary/additive paths, deduplication, repeated bootstrap, other requested layers, debug preference, overflow and missing-file guards; no system settings changed.");return 0;
}

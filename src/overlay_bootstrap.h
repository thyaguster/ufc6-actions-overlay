#ifndef UFC6_OVERLAY_BOOTSTRAP_H
#define UFC6_OVERLAY_BOOTSTRAP_H
#include <windows.h>
#include <wchar.h>
static int overlay_env_has(const wchar_t *list,const wchar_t *value) {
    size_t length=wcslen(value);
    for(const wchar_t *start=list;*start;){
        const wchar_t *end=wcschr(start,L';');size_t count=end?(size_t)(end-start):wcslen(start);
        if(count==length&&!_wcsnicmp(start,value,length))return 1;
        if(!end)break;start=end+1;
    }
    return 0;
}
/* Merge inherited explicit paths without duplicates or truncation. The legacy
 * search variable works on loaders predating VK_ADD_LAYER_PATH support. */
static int overlay_path_merge(wchar_t *value,size_t capacity,const wchar_t *list){
 for(const wchar_t *start=list;*start;){
  const wchar_t *end=wcschr(start,L';');size_t count=end?(size_t)(end-start):wcslen(start);
  if(count){wchar_t part[16000];if(count>=16000)return 0;wmemcpy(part,start,count);part[count]=0;
   if(!overlay_env_has(value,part)){size_t used=wcslen(value);if(used+count+2>capacity)return 0;value[used++]=L';';wmemcpy(value+used,part,count+1);}
  }
  if(!end)break;start=end+1;
 }
 return 1;
}
/* Only this process: no registry, launcher edits, or system loader changes. */
/* Keep the large environment buffers out of DllMain's THREAD_ATTACH stack frame.
 * Some GPU-driver worker threads use very small stacks. */
static __declspec(noinline) void overlay_bootstrap(HMODULE module) {
    wchar_t directory[MAX_PATH],existing[16000],value[16384];
    if(!GetModuleFileNameW(module,directory,MAX_PATH))return;
    wchar_t *slash=wcsrchr(directory,L'\\');if(!slash)return;*slash=0;
    /* An old uninstaller may remove only the old four files. Forward the runtime
     * normally, without enabling a missing Vulkan layer, in that case. */
    const wchar_t *required[]={L"version.dll",L"ufc6_menu_layer.dll",L"ufc6_menu_layer.json"};
    for(unsigned i=0;i<sizeof(required)/sizeof(required[0]);++i){
        wchar_t path[MAX_PATH];
        if(wcslen(directory)+wcslen(required[i])+2>MAX_PATH)return;
        lstrcpyW(path,directory);lstrcatW(path,L"\\");lstrcatW(path,required[i]);
        DWORD attributes=GetFileAttributesW(path);
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return;
    }
    lstrcpyW(value,directory);
    DWORD n=GetEnvironmentVariableW(L"VK_LAYER_PATH",existing,16000);
    int complete=n<16000&&overlay_path_merge(value,16384,n?existing:L"");
    n=GetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",existing,16000);
    complete=complete&&n<16000&&overlay_path_merge(value,16384,n?existing:L"");
    if(complete)SetEnvironmentVariableW(L"VK_LAYER_PATH",value);
    /* Keep the additive setting too, without changing inherited values. */
    if(n<16000&&(!n||!overlay_env_has(existing,directory))){lstrcpyW(value,directory);if(n){lstrcatW(value,L";");lstrcatW(value,existing);}SetEnvironmentVariableW(L"VK_ADD_LAYER_PATH",value);}
    if(!GetEnvironmentVariableW(L"VK_LOADER_DEBUG",NULL,0))SetEnvironmentVariableW(L"VK_LOADER_DEBUG",L"error,warn");
    n=GetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",existing,16000);
    if(n<16000&&(!n||!overlay_env_has(existing,L"VK_LAYER_UFC6_quit_menu"))){lstrcpyW(value,L"VK_LAYER_UFC6_quit_menu");if(n){lstrcatW(value,L";");lstrcatW(value,existing);}SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",value);}
}
#endif

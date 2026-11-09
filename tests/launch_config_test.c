#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include "../src/launch_config.h"
int wmain(int argc,wchar_t **argv){
    assert(argc==2);wchar_t root[1024],bin[1100],ini[1200],self[1024],exe[1100];
    assert(GetFullPathNameW(argv[1],1024,root,NULL));CreateDirectoryW(root,NULL);
    swprintf(bin,1100,L"%ls\\bin",root);assert(CreateDirectoryW(bin,NULL));
    swprintf(ini,1200,L"%ls\\ufc6-quit-menu.ini",bin);GetModuleFileNameW(NULL,self,1024);
    swprintf(exe,1100,L"%ls\\UFC6.exe",root);assert(CopyFileW(self,exe,TRUE));
    LaunchConfig c,loaded;assert(launch_config_load(bin,&c)&&!c.custom&&c.wait_parent&&launch_config_valid(&c));
    assert(!wcscmp(c.arguments,L"--play --headless --ui-lang en"));
    assert(WritePrivateProfileStringW(L"Menu",L"UIScale",L"175",ini));
    assert(WritePrivateProfileStringW(L"Other",L"Keep",L"untouched",ini));
    c.custom=1;c.wait_parent=0;wcscpy(c.arguments,L"\"C:\\game data\\ação.elf\" --label \"a \\\"quote\\\"\"");
    assert(launch_config_save(ini,&c));assert(launch_config_load(bin,&loaded));
    assert(loaded.custom&&!loaded.wait_parent&&!wcscmp(loaded.arguments,c.arguments)&&!wcscmp(loaded.executable,c.executable));
    assert(GetPrivateProfileIntW(L"Menu",L"UIScale",0,ini)==175);
    wchar_t value[100];GetPrivateProfileStringW(L"Other",L"Keep",L"",value,100,ini);assert(!wcscmp(value,L"untouched"));
    wchar_t command[LAUNCH_PATH+LAUNCH_ARGS+8];assert(launch_config_command(&loaded,command,sizeof(command)/sizeof(command[0])));
    assert(wcsstr(command,L"ação.elf")&&wcsstr(command,L"--label"));assert(!launch_config_command(&loaded,command,5));
    wcscpy(c.executable,L"relative.exe");assert(!launch_config_valid(&c)&&!launch_config_save(ini,&c));
    assert(launch_config_load(bin,&c)&&!wcscmp(c.arguments,loaded.arguments));
    assert(WritePrivateProfileStringW(L"Launch",L"WorkingDirectory",L"",ini));assert(launch_config_load(bin,&c)&&!wcscmp(c.directory,root));
    assert(WritePrivateProfileStringW(L"Launch",L"Mode",L"unknown",ini));assert(!launch_config_load(bin,&c));
    DeleteFileW(ini);DeleteFileW(exe);RemoveDirectoryW(bin);RemoveDirectoryW(root);
    puts("PASS: PORTHEON defaults, Unicode/quoted arguments, atomic persistence, unrelated settings, invalid/missing paths and working-folder fallback.");return 0;
}

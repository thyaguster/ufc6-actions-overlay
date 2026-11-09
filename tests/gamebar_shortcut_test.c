/* Exercise real registry semantics under a unique fixture key only. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/gamebar_shortcut.c"
int main(void){
    wchar_t path[160];swprintf(path,160,L"Software\\UFC6ActionsOverlayTest-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    HKEY key=NULL;
    assert(RegOpenKeyExW(HKEY_CURRENT_USER,path,0,KEY_READ,&key)==ERROR_FILE_NOT_FOUND);
    assert(gamebar_read_at(path)==1); /* Read does not create a key or value. */
    assert(RegOpenKeyExW(HKEY_CURRENT_USER,path,0,KEY_READ,&key)==ERROR_FILE_NOT_FOUND);
    assert(gamebar_write_at(path,0)==ERROR_SUCCESS&&gamebar_read_at(path)==0);
    assert(gamebar_write_at(path,1)==ERROR_SUCCESS&&gamebar_read_at(path)==1);
    assert(RegOpenKeyExW(HKEY_CURRENT_USER,path,0,KEY_SET_VALUE|KEY_QUERY_VALUE,&key)==ERROR_SUCCESS);
    const wchar_t invalid[]=L"preserve me";
    assert(RegSetValueExW(key,gamebar_value,0,REG_SZ,(const BYTE*)invalid,sizeof(invalid))==ERROR_SUCCESS);
    assert(gamebar_read_at(path)==-1&&gamebar_write_at(path,0)==ERROR_INVALID_DATA);
    wchar_t check[40];DWORD size=sizeof(check),type;
    assert(RegQueryValueExW(key,gamebar_value,NULL,&type,(BYTE*)check,&size)==ERROR_SUCCESS&&type==REG_SZ&&!wcscmp(check,invalid));
    DWORD bad=5;assert(RegSetValueExW(key,gamebar_value,0,REG_DWORD,(const BYTE*)&bad,sizeof(bad))==ERROR_SUCCESS);
    assert(gamebar_read_at(path)==-1&&gamebar_write_at(path,0)==ERROR_INVALID_DATA);
    assert(RegDeleteValueW(key,gamebar_value)==ERROR_SUCCESS);RegCloseKey(key);
    assert(gamebar_read_at(path)==1);
    assert(RegDeleteKeyW(HKEY_CURRENT_USER,path)==ERROR_SUCCESS);
    puts("PASS: Game Bar preference absent defaults, explicit On/Off, external changes and unknown-value preservation; only an isolated fixture key was changed.");
    return 0;
}

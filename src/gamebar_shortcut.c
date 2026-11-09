#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamebar_shortcut.h"
/* Microsoft's Windows 11 settings reference maps this value to
 * Gaming > Game Bar > Allow your controller to open Game Bar.
 * Read only at startup; writes happen only from the explicit settings action. */
static const wchar_t gamebar_key[]=L"Software\\Microsoft\\GameBar";
static const wchar_t gamebar_value[]=L"UseNexusForGameBarEnabled";
static int gamebar_read_at(const wchar_t *path){
    HKEY key;LONG result=RegOpenKeyExW(HKEY_CURRENT_USER,path,0,KEY_QUERY_VALUE,&key);
    if(result==ERROR_FILE_NOT_FOUND)return 1;
    if(result!=ERROR_SUCCESS)return -1;
    DWORD type=0,value=0,size=sizeof(value);
    result=RegQueryValueExW(key,gamebar_value,NULL,&type,(BYTE*)&value,&size);RegCloseKey(key);
    if(result==ERROR_FILE_NOT_FOUND)return 1;
    if(result!=ERROR_SUCCESS||type!=REG_DWORD||size!=sizeof(value)||value>1)return -1;
    return (int)value;
}
static LONG gamebar_write_at(const wchar_t *path,int enabled){
    if(gamebar_read_at(path)<0)return ERROR_INVALID_DATA;
    HKEY key;LONG result=RegCreateKeyExW(HKEY_CURRENT_USER,path,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);
    if(result!=ERROR_SUCCESS)return result;
    DWORD value=enabled!=0;
    result=RegSetValueExW(key,gamebar_value,0,REG_DWORD,(const BYTE*)&value,sizeof(value));RegCloseKey(key);
    return result;
}
int gamebar_shortcut_read(void){return gamebar_read_at(gamebar_key);}
LONG gamebar_shortcut_write(int enabled){return gamebar_write_at(gamebar_key,enabled);}

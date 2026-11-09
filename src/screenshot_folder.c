#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <wchar.h>
#include "screenshot_folder.h"
static const GUID screenshots={0xb7bede81,0xdf94,0x4682,{0xa7,0xd8,0x57,0xa5,0x26,0x20,0xb8,0x6f}};
static const GUID pictures_id={0x33e28130,0x4e1e,0x4676,{0x83,0x5a,0x98,0x39,0x5c,0x3b,0xc3,0xbb}};
PWSTR menu_screenshot_directory(void) {
    PWSTR directory=NULL;
    if(SUCCEEDED(SHGetKnownFolderPath(&screenshots,KF_FLAG_CREATE,NULL,&directory)))return directory;
    if(directory)CoTaskMemFree(directory);
    PWSTR pictures=NULL;
    if(FAILED(SHGetKnownFolderPath(&pictures_id,KF_FLAG_CREATE,NULL,&pictures)))return NULL;
    size_t capacity=wcslen(pictures)+13;
    directory=CoTaskMemAlloc(capacity*sizeof(wchar_t));
    if(directory){wcscpy(directory,pictures);wcscat(directory,L"\\Screenshots");}
    CoTaskMemFree(pictures);
    if(directory&&!CreateDirectoryW(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){
        CoTaskMemFree(directory);directory=NULL;
    }
    return directory;
}

/* Hidden native controls exercise the real dialog procedure and save path. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/launcher_dialog.c"
int wmain(int argc,wchar_t **argv){
    assert(argc==2);wchar_t root[1024],bin[1100],ini[1200],exe[1100],self[1024];
    assert(GetFullPathNameW(argv[1],1024,root,NULL));assert(CreateDirectoryW(root,NULL));
    swprintf(bin,1100,L"%ls\\bin",root);assert(CreateDirectoryW(bin,NULL));swprintf(ini,1200,L"%ls\\settings.ini",bin);
    swprintf(exe,1100,L"%ls\\UFC6.exe",root);GetModuleFileNameW(NULL,self,1024);assert(CopyFileW(self,exe,TRUE));
    DialogState state={.ini=ini};lstrcpynW(state.bin,bin,LAUNCH_PATH);launch_config_default(bin,&state.config);
    union {DWORD align;BYTE data[256];} storage={0};DLGTEMPLATE *type=(DLGTEMPLATE*)storage.data;
    type->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;type->cx=380;type->cy=180;
    WORD *next=(WORD*)(type+1);*next++=0;*next++=0;wcscpy((wchar_t*)next,L"Launcher settings fixture");
    HWND window=CreateDialogIndirectParamW(GetModuleHandleW(NULL),type,NULL,procedure,(LPARAM)&state);assert(window);
    assert(!IsWindowEnabled(GetDlgItem(window,EXE))&&IsDlgButtonChecked(window,WAIT_PARENT)==BST_CHECKED);
    SendDlgItemMessageW(window,CUSTOM,BM_CLICK,0,0);assert(IsWindowEnabled(GetDlgItem(window,EXE))&&IsDlgButtonChecked(window,WAIT_PARENT)==BST_UNCHECKED);
    SetDlgItemTextW(window,ARGS,L"--custom-test \"a quoted argument\"");SetDlgItemTextW(window,CWD,L"");
    SetWindowPos(window,NULL,-32000,-32000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);ShowWindow(window,SW_SHOWNOACTIVATE);UpdateWindow(window);
    RECT rect;GetWindowRect(window,&rect);int width=rect.right-rect.left,height=rect.bottom-rect.top;
    HDC dc=CreateCompatibleDC(NULL);BITMAPINFO info={.bmiHeader={.biSize=sizeof(BITMAPINFOHEADER),.biWidth=width,.biHeight=-height,.biPlanes=1,.biBitCount=32,.biCompression=BI_RGB}};
    void *pixels;HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);HGDIOBJ old=SelectObject(dc,bmp);SendMessageW(window,WM_PRINT,(WPARAM)dc,PRF_NONCLIENT|PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    BITMAPFILEHEADER header={.bfType=0x4d42,.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER),.bfSize=(DWORD)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+width*height*4)};
    FILE *image=fopen(".build/launcher-dialog.bmp","wb");assert(image);fwrite(&header,sizeof(header),1,image);fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,image);fwrite(pixels,width*height*4,1,image);fclose(image);
    SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
    SendMessageW(window,WM_COMMAND,IDOK,0);LaunchConfig loaded;assert(launch_config_read(ini,bin,&loaded)&&loaded.custom&&!loaded.wait_parent&&!wcscmp(loaded.arguments,L"--custom-test \"a quoted argument\"")&&!wcscmp(loaded.directory,root));
    DestroyWindow(window);DeleteFileW(ini);DeleteFileW(exe);RemoveDirectoryW(bin);RemoveDirectoryW(root);
    puts("PASS: native launcher controls, custom/default gating, persistent-launcher default, argument save and blank working-folder fallback.");return 0;
}

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <wchar.h>
#include "launch_config.h"
#include "launcher_dialog.h"
enum { CUSTOM=100,EXE,ARGS,CWD,WAIT_PARENT,BROWSE };
typedef struct { const wchar_t *ini; wchar_t bin[LAUNCH_PATH]; LaunchConfig config; HFONT font; } DialogState;
static HWND control(HWND dialog,const wchar_t *type,const wchar_t *label,DWORD style,int x,int y,int width,int height,int id){
    HWND window=CreateWindowExW(!wcscmp(type,L"EDIT")?WS_EX_CLIENTEDGE:0,type,label,WS_CHILD|WS_VISIBLE|style,x,y,width,height,dialog,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    DialogState *state=(DialogState*)GetWindowLongPtrW(dialog,GWLP_USERDATA);
    SendMessageW(window,WM_SETFONT,(WPARAM)(state&&state->font?state->font:GetStockObject(DEFAULT_GUI_FONT)),TRUE);return window;
}
static void populate(HWND window,const LaunchConfig *c){
    SetDlgItemTextW(window,EXE,c->executable);SetDlgItemTextW(window,ARGS,c->arguments);SetDlgItemTextW(window,CWD,c->directory);
    CheckDlgButton(window,CUSTOM,c->custom?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(window,WAIT_PARENT,c->wait_parent?BST_CHECKED:BST_UNCHECKED);
    const int ids[]={EXE,ARGS,CWD,WAIT_PARENT,BROWSE};for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);++i)EnableWindow(GetDlgItem(window,ids[i]),c->custom);
}
static INT_PTR CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    DialogState *state=(DialogState*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_INITDIALOG){
        state=(DialogState*)lp;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)state);
        state->font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        RECT client;GetClientRect(window,&client);int width=client.right;
        control(window,L"STATIC",L"Restart Game and post-reboot auto-launch use these settings.",0,16,12,width-32,20,0);
        control(window,L"BUTTON",L"Use a custom launch executable",BS_AUTOCHECKBOX|WS_TABSTOP,16,40,width-32,24,CUSTOM);
        control(window,L"STATIC",L"Executable (launcher or game engine)",0,16,74,width-32,18,0);
        control(window,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,16,94,width-125,24,EXE);
        control(window,L"BUTTON",L"Browse...",WS_TABSTOP, width-99,94,83,24,BROWSE);
        control(window,L"STATIC",L"Launch arguments",0,16,130,width-32,18,0);
        control(window,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,16,150,width-32,24,ARGS);
        control(window,L"STATIC",L"Start in (optional; defaults to the executable's folder)",0,16,186,width-32,18,0);
        control(window,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,16,206,width-32,24,CWD);
        control(window,L"BUTTON",L"Wait for the previous launcher to exit before restarting",BS_AUTOCHECKBOX|WS_TABSTOP,16,244,width-32,24,WAIT_PARENT);
        control(window,L"STATIC",L"Leave this Off for launchers that stay open. PORTHEON waits for cleanup.",0,16,274,width-32,28,0);
        control(window,L"BUTTON",L"Save",BS_DEFPUSHBUTTON|WS_TABSTOP,width-196,314,82,28,IDOK);
        control(window,L"BUTTON",L"Cancel",WS_TABSTOP,width-98,314,82,28,IDCANCEL);
        SendDlgItemMessageW(window,EXE,EM_SETLIMITTEXT,LAUNCH_PATH-1,0);SendDlgItemMessageW(window,ARGS,EM_SETLIMITTEXT,LAUNCH_ARGS-1,0);SendDlgItemMessageW(window,CWD,EM_SETLIMITTEXT,LAUNCH_PATH-1,0);
        populate(window,&state->config);SetForegroundWindow(window);return TRUE;
    }
    if(message==WM_NCDESTROY){if(state&&state->font){DeleteObject(state->font);state->font=NULL;}return FALSE;}
    if(message==WM_CLOSE){EndDialog(window,IDCANCEL);return TRUE;}
    if(message!=WM_COMMAND||!state)return FALSE;
    int id=LOWORD(wp);
    if(id==IDCANCEL){EndDialog(window,IDCANCEL);return TRUE;}
    if(id==CUSTOM){
        if(IsDlgButtonChecked(window,CUSTOM)==BST_UNCHECKED){LaunchConfig defaults;launch_config_default(state->bin,&defaults);populate(window,&defaults);}
        else {const int ids[]={EXE,ARGS,CWD,WAIT_PARENT,BROWSE};for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);++i)EnableWindow(GetDlgItem(window,ids[i]),TRUE);CheckDlgButton(window,WAIT_PARENT,BST_UNCHECKED);}
        return TRUE;
    }
    if(id==BROWSE){
        wchar_t path[LAUNCH_PATH]=L"";
        OPENFILENAMEW ofn={.lStructSize=sizeof(ofn),.hwndOwner=window,.lpstrFilter=L"Windows executables (*.exe)\0*.exe\0\0",.lpstrFile=path,.nMaxFile=LAUNCH_PATH,.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER};
        if(GetOpenFileNameW(&ofn)){SetDlgItemTextW(window,EXE,path);SetDlgItemTextW(window,CWD,L"");}return TRUE;
    }
    if(id==IDOK){
        LaunchConfig config;
        if(IsDlgButtonChecked(window,CUSTOM)==BST_UNCHECKED)launch_config_default(state->bin,&config);
        else {
            ZeroMemory(&config,sizeof(config));config.custom=1;config.wait_parent=IsDlgButtonChecked(window,WAIT_PARENT)==BST_CHECKED;
            GetDlgItemTextW(window,EXE,config.executable,LAUNCH_PATH);GetDlgItemTextW(window,ARGS,config.arguments,LAUNCH_ARGS);GetDlgItemTextW(window,CWD,config.directory,LAUNCH_PATH);
            if(!config.directory[0]){lstrcpynW(config.directory,config.executable,LAUNCH_PATH);wchar_t *slash=wcsrchr(config.directory,L'\\');if(slash)*slash=0;}
        }
        if(!launch_config_valid(&config)){MessageBoxW(window,L"Choose an existing .exe and a valid start folder. Arguments must fit on one line.",L"Check launcher settings",MB_OK|MB_ICONWARNING);return TRUE;}
        if(!launch_config_save(state->ini,&config)){MessageBoxW(window,L"Could not save launcher settings. The previous configuration was kept.",L"Launcher settings",MB_OK|MB_ICONERROR);return TRUE;}
        EndDialog(window,IDOK);return TRUE;
    }
    return FALSE;
}
BOOL launcher_settings_dialog(HWND game,HMODULE module,const wchar_t *ini){
    DialogState state={.ini=ini};GetModuleFileNameW(module,state.bin,LAUNCH_PATH);wchar_t *slash=wcsrchr(state.bin,L'\\');if(!slash)return FALSE;*slash=0;
    if(!launch_config_read(ini,state.bin,&state.config))launch_config_default(state.bin,&state.config);
    union { DWORD align; BYTE data[256]; } storage={0};DLGTEMPLATE *dialog=(DLGTEMPLATE*)storage.data;
    dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;dialog->cx=380;dialog->cy=180;
    WORD *next=(WORD*)(dialog+1);*next++=0;*next++=0;wcscpy((wchar_t*)next,L"UFC 6 launcher settings");
    BOOL minimized=IsIconic(game);if(!minimized)ShowWindow(game,SW_MINIMIZE);
    INT_PTR result=DialogBoxIndirectParamW(module,dialog,NULL,procedure,(LPARAM)&state);
    if(!minimized){ShowWindow(game,SW_RESTORE);SetForegroundWindow(game);}
    return result==IDOK;
}

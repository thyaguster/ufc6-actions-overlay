/* PS5 Pro host: mute only this process's independent Windows audio sessions. */
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <audioclient.h>
#include <wchar.h>
#include "audio_session_mute.h"

enum { MAX_OWNED_SESSIONS=16, MAX_SESSION_ID=260 };
typedef struct { wchar_t id[MAX_SESSION_ID]; ISimpleAudioVolume *volume; } OwnedSession;
static OwnedSession owned[MAX_OWNED_SESSIONS];
static unsigned owned_count;
static int com_ready,com_owned;
static ULONGLONG next_scan;
static const GUID mute_context={0xddcf3087,0xa243,0x4722,{0xb1,0x76,0x5d,0x94,0xa2,0x4e,0x61,0x38}};

static int ensure_com(void){
    if(com_ready)return 1;
    HRESULT result=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(SUCCEEDED(result)){com_ready=1;com_owned=1;return 1;}
    if(result==RPC_E_CHANGED_MODE){com_ready=1;return 1;}
    return 0;
}

static void restore_owned(void){
    for(unsigned i=0;i<owned_count;++i){
        ISimpleAudioVolume *volume=owned[i].volume;BOOL muted=FALSE;
        if(volume){
            if(SUCCEEDED(volume->lpVtbl->GetMute(volume,&muted))&&muted)
                volume->lpVtbl->SetMute(volume,FALSE,&mute_context);
            volume->lpVtbl->Release(volume);
        }
        owned[i]=(OwnedSession){0};
    }
    owned_count=0;
}

static int already_owned(const wchar_t *id){
    for(unsigned i=0;i<owned_count;++i)if(!wcscmp(id,owned[i].id))return 1;
    return 0;
}

static void consider_session(IAudioSessionControl *session){
    IAudioSessionControl2 *details=NULL;
    if(FAILED(session->lpVtbl->QueryInterface(session,&IID_IAudioSessionControl2,(void**)&details)))return;
    DWORD pid=0;HRESULT pid_result=details->lpVtbl->GetProcessId(details,&pid);
    if(pid_result!=S_OK||pid!=GetCurrentProcessId()||owned_count>=MAX_OWNED_SESSIONS){details->lpVtbl->Release(details);return;}
    LPWSTR id=NULL;HRESULT id_result=details->lpVtbl->GetSessionInstanceIdentifier(details,&id);
    if(FAILED(id_result)||!id||wcslen(id)>=MAX_SESSION_ID||already_owned(id)){
        if(id)CoTaskMemFree(id);details->lpVtbl->Release(details);return;
    }
    ISimpleAudioVolume *volume=NULL;BOOL muted=FALSE;
    if(SUCCEEDED(session->lpVtbl->QueryInterface(session,&IID_ISimpleAudioVolume,(void**)&volume))&&volume){
        if(SUCCEEDED(volume->lpVtbl->GetMute(volume,&muted))&&!muted&&
           SUCCEEDED(volume->lpVtbl->SetMute(volume,TRUE,&mute_context))){
            OwnedSession *entry=&owned[owned_count++];
            wcscpy(entry->id,id);entry->volume=volume;volume=NULL;
        }
        if(volume)volume->lpVtbl->Release(volume);
    }
    CoTaskMemFree(id);details->lpVtbl->Release(details);
}

static void scan_endpoint(IMMDevice *device){
    IAudioSessionManager2 *manager=NULL;
    if(FAILED(device->lpVtbl->Activate(device,&IID_IAudioSessionManager2,CLSCTX_ALL,NULL,(void**)&manager)))return;
    IAudioSessionEnumerator *sessions=NULL;
    if(SUCCEEDED(manager->lpVtbl->GetSessionEnumerator(manager,&sessions))&&sessions){
        int count=0;if(SUCCEEDED(sessions->lpVtbl->GetCount(sessions,&count))){
            for(int i=0;i<count&&owned_count<MAX_OWNED_SESSIONS;++i){
                IAudioSessionControl *session=NULL;
                if(SUCCEEDED(sessions->lpVtbl->GetSession(sessions,i,&session))&&session){
                    consider_session(session);session->lpVtbl->Release(session);
                }
            }
        }
        sessions->lpVtbl->Release(sessions);
    }
    manager->lpVtbl->Release(manager);
}

static void scan_sessions(void){
    IMMDeviceEnumerator *enumerator=NULL;
    if(FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_ALL,&IID_IMMDeviceEnumerator,(void**)&enumerator)))return;
    IMMDeviceCollection *devices=NULL;
    if(SUCCEEDED(enumerator->lpVtbl->EnumAudioEndpoints(enumerator,eRender,DEVICE_STATE_ACTIVE,&devices))&&devices){
        UINT count=0;
        if(SUCCEEDED(devices->lpVtbl->GetCount(devices,&count))){
            for(UINT i=0;i<count&&owned_count<MAX_OWNED_SESSIONS;++i){
                IMMDevice *device=NULL;
                if(SUCCEEDED(devices->lpVtbl->Item(devices,i,&device))&&device){
                    scan_endpoint(device);device->lpVtbl->Release(device);
                }
            }
        }
        devices->lpVtbl->Release(devices);
    }
    enumerator->lpVtbl->Release(enumerator);
}

void audio_session_mute_poll(int enabled,int focused){
    if(!enabled||focused){restore_owned();next_scan=0;return;}
    ULONGLONG now=GetTickCount64();if(now<next_scan)return;
    next_scan=now+250;
    if(ensure_com())scan_sessions();
}

void audio_session_mute_close(void){
    restore_owned();
    if(com_owned)CoUninitialize();
    com_owned=0;com_ready=0;next_scan=0;
}

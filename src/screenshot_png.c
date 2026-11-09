/* Encode only GPU readback pixels. Desktop windows never enter the screenshot. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdint.h>
#include <stdio.h>
#include "screenshot_png.h"
#include "screenshot_folder.h"

BOOL menu_save_png_path(HMODULE module,uint32_t width,uint32_t height,unsigned char *pixels,int rgba,PWSTR *saved_path) {
    if(saved_path)*saved_path=NULL;
    (void)module;
    if(!width||!height||width>UINT32_MAX/height/4)return FALSE;
    size_t bytes=(size_t)width*height*4;
    for(size_t i=0;i<bytes;i+=4){
        if(rgba){unsigned char b=pixels[i];pixels[i]=pixels[i+2];pixels[i+2]=b;}
        pixels[i+3]=255;
    }
    HRESULT init=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)return FALSE;
    IWICImagingFactory *factory=NULL;IWICBitmapEncoder *encoder=NULL;
    IWICBitmapFrameEncode *frame=NULL;IPropertyBag2 *properties=NULL;IStream *stream=NULL;
    HRESULT hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void **)&factory);
    if(SUCCEEDED(hr))hr=CreateStreamOnHGlobal(NULL,TRUE,&stream);
    if(SUCCEEDED(hr))hr=IWICImagingFactory_CreateEncoder(factory,&GUID_ContainerFormatPng,NULL,&encoder);
    if(SUCCEEDED(hr))hr=IWICBitmapEncoder_Initialize(encoder,stream,WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=IWICBitmapEncoder_CreateNewFrame(encoder,&frame,&properties);
    if(SUCCEEDED(hr))hr=IWICBitmapFrameEncode_Initialize(frame,properties);
    if(SUCCEEDED(hr))hr=IWICBitmapFrameEncode_SetSize(frame,width,height);
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(SUCCEEDED(hr))hr=IWICBitmapFrameEncode_SetPixelFormat(frame,&format);
    if(SUCCEEDED(hr)&&!IsEqualGUID(&format,&GUID_WICPixelFormat32bppBGRA))hr=E_FAIL;
    if(SUCCEEDED(hr))hr=IWICBitmapFrameEncode_WritePixels(frame,height,width*4,(UINT)bytes,pixels);
    if(SUCCEEDED(hr))hr=IWICBitmapFrameEncode_Commit(frame);
    if(SUCCEEDED(hr))hr=IWICBitmapEncoder_Commit(encoder);
    BOOL saved=FALSE;
    if(SUCCEEDED(hr)){
        HGLOBAL memory=NULL;STATSTG stat;
        hr=GetHGlobalFromStream(stream,&memory);
        if(SUCCEEDED(hr))hr=IStream_Stat(stream,&stat,STATFLAG_NONAME);
        PWSTR directory=SUCCEEDED(hr)?menu_screenshot_directory():NULL;
        if(directory&&stat.cbSize.QuadPart<=UINT32_MAX){
            size_t capacity=wcslen(directory)+128;
            PWSTR path=CoTaskMemAlloc(capacity*sizeof(wchar_t));
            if(path){
                SYSTEMTIME now;GetLocalTime(&now);
                swprintf(path,capacity,L"%ls\\UFC6_%04u-%02u-%02u_%02u-%02u-%02u-%03u_%llu.png",directory,
                    now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,(unsigned long long)GetTickCount64());
                HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
                if(file!=INVALID_HANDLE_VALUE){
                    void *data=GlobalLock(memory);DWORD written=0;
                    if(data){saved=WriteFile(file,data,(DWORD)stat.cbSize.QuadPart,&written,NULL)&&written==stat.cbSize.QuadPart;GlobalUnlock(memory);}
                    CloseHandle(file);if(!saved)DeleteFileW(path);
                }
                if(saved&&saved_path)*saved_path=path;else CoTaskMemFree(path);
            }
        }
        if(directory)CoTaskMemFree(directory);
    }
    if(properties)IPropertyBag2_Release(properties);if(frame)IWICBitmapFrameEncode_Release(frame);
    if(encoder)IWICBitmapEncoder_Release(encoder);if(stream)IStream_Release(stream);
    if(factory)IWICImagingFactory_Release(factory);if(SUCCEEDED(init))CoUninitialize();
    return saved;
}

BOOL menu_save_png(HMODULE module,uint32_t width,uint32_t height,unsigned char *pixels,int rgba){return menu_save_png_path(module,width,height,pixels,rgba,NULL);}

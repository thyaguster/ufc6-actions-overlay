#ifndef UFC6_ENGINE_COMPAT_H
#define UFC6_ENGINE_COMPAT_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

/* A new file hash is acceptable only when the controller callback layout is
 * still uniquely recognizable. Every hook also verifies its exact bytes. */
static INIT_ONCE engine_identity_once=INIT_ONCE_STATIC_INIT;
static int engine_identity_profile;
static uintptr_t engine_identity_button_rva;

static BOOL CALLBACK engine_identity_detect(PINIT_ONCE once,PVOID parameter,PVOID *context){
    (void)once;(void)parameter;(void)context;
    wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(NULL,path,MAX_PATH);
    if(!length||length>=MAX_PATH)return TRUE;
    const wchar_t *name=wcsrchr(path,L'\\');name=name?name+1:path;
    const BYTE *base=(const BYTE*)GetModuleHandleW(NULL);
    if(!base)return TRUE;
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER*)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x1000)return TRUE;
    const IMAGE_NT_HEADERS64 *nt=(const IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
       nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
       nt->OptionalHeader.SizeOfImage<0x100000||nt->OptionalHeader.SizeOfImage>0x80000000||
       !nt->FileHeader.NumberOfSections||nt->FileHeader.NumberOfSections>32)return TRUE;
    const size_t image=nt->OptionalHeader.SizeOfImage;
    if(!lstrcmpiW(name,L"UFC 6.exe")&&image==0x3c03000&&nt->FileHeader.TimeDateStamp==0x6adf7fd0){
        static const BYTE button[]={0x55,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
        static const BYTE axis[]={0x55,0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xd0,0,0,0,0x48,0x8d,0xac,0x24,0x80,0,0,0};
        static const BYTE disconnect[]={0x55,0x56,0x57,0x48,0x83,0xec,0x30,0x48,0x8d,0x6c,0x24,0x30,0x48,0xc7,0x45,0xf8,0xfe,0xff,0xff,0xff};
        if(!memcmp(base+0x126f040,button,sizeof(button))&&!memcmp(base+0x126f270,axis,sizeof(axis))&&
           !memcmp(base+0x126ef60,disconnect,sizeof(disconnect)))engine_identity_profile=1;
        return TRUE;
    }
    if(!lstrcmpiW(name,L"UFC 6 Parsec.exe")&&image==0x3c05000&&nt->FileHeader.TimeDateStamp==0x6adf7fd0){
        static const BYTE button[]={0xe9,0x56,0x40,0x99,0x02},axis[]={0xe9,0x17,0x3e,0x99,0x02},disconnect[]={0xe9,0xb0,0x41,0x99,0x02};
        if(!memcmp(base+0x126f040,button,sizeof(button))&&!memcmp(base+0x126f270,axis,sizeof(axis))&&
           !memcmp(base+0x126ef60,disconnect,sizeof(disconnect)))engine_identity_profile=2;
        return TRUE;
    }
    if(lstrcmpiW(name,L"UFC 6 novo.exe"))return TRUE;
    static const BYTE button[]={0x55,0x41,0x57,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xec,0xc8,0,0,0};
    static const BYTE axis[]={0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0xc8,0,0,0};
    static const BYTE disconnect[]={0x55,0x41,0x56,0x56,0x57,0x53,0x48,0x83,0xec,0x20,0x48,0x8d,0x6c,0x24,0x20};
    const IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    uintptr_t found=0;unsigned matches=0;
    for(unsigned section=0;section<nt->FileHeader.NumberOfSections;++section){
        const IMAGE_SECTION_HEADER *s=sections+section;
        size_t start=s->VirtualAddress,size=s->Misc.VirtualSize;
        if(!(s->Characteristics&IMAGE_SCN_MEM_EXECUTE)||start>=image||size>image-start||size<0x400||size>0x4000000)continue;
        const BYTE *code=base+start;
        for(size_t offset=0x380;offset+0x190+sizeof(axis)<=size;++offset){
            if(code[offset]!=button[0]||memcmp(code+offset,button,sizeof(button)))continue;
            if(memcmp(code+offset+0x190,axis,sizeof(axis))||memcmp(code+offset-0x380,disconnect,sizeof(disconnect)))continue;
            found=start+offset;if(++matches>1)return TRUE;
        }
    }
    if(matches==1){engine_identity_profile=3;engine_identity_button_rva=found;}
    return TRUE;
}
static int engine_fingerprint(void){InitOnceExecuteOnce(&engine_identity_once,engine_identity_detect,NULL,NULL);return engine_identity_profile;}
static inline uintptr_t engine_new_button_rva(void){InitOnceExecuteOnce(&engine_identity_once,engine_identity_detect,NULL,NULL);return engine_identity_button_rva;}
#endif

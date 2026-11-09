#ifndef UFC6_ENGINE_FINGERPRINT_H
#define UFC6_ENGINE_FINGERPRINT_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <string.h>
/* Exact file identities; unknown revisions never receive controller hooks. */
static int engine_fingerprint(void) {
    static const char *hashes[]={
        "f3c42126bd9df5c12cb69c1c57233a94ba024c50929ba0dfe20e715521f691a2",
        "ad94dc7e30ad12c8078d14d96520c1d48d4b0c658604cc3dcc1d68a3e228773d"
    };
    wchar_t path[MAX_PATH];if(!GetModuleFileNameW(NULL,path,MAX_PATH))return 0;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);if(file==INVALID_HANDLE_VALUE)return 0;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;int profile=0;
    if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)||!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))goto done;
    unsigned char buffer[65536],digest[32];DWORD read,size=32;
    for(;;){if(!ReadFile(file,buffer,sizeof(buffer),&read,NULL))goto done;if(!read)break;if(!CryptHashData(hash,buffer,read,0))goto done;}
    if(!CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)||size!=32)goto done;
    char text[65];static const char hex[]="0123456789abcdef";
    for(int i=0;i<32;++i){text[i*2]=hex[digest[i]>>4];text[i*2+1]=hex[digest[i]&15];}text[64]=0;
    for(int i=0;i<2;++i)if(!strcmp(text,hashes[i]))profile=i+1;
done:
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return profile;
}
#endif

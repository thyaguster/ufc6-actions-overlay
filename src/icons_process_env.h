#ifndef UFC6_ICONS_PROCESS_ENV_H
#define UFC6_ICONS_PROCESS_ENV_H
#include <windows.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
/* Every invocation is an independent one-file application. A game relaunched
   by a previous helper can otherwise inherit that helper's deleted _MEI path.
   Use a child-only environment block; never mutate the game's environment. */
static int icons_env_compare(const void *a,const void *b){return _wcsicmp(*(const wchar_t *const*)a,*(const wchar_t *const*)b);}
static wchar_t *icons_process_environment(void){
    wchar_t *source=GetEnvironmentStringsW();if(!source)return NULL;
    const wchar_t key[]=L"PYINSTALLER_RESET_ENVIRONMENT=";
    const wchar_t reset[]=L"PYINSTALLER_RESET_ENVIRONMENT=1";
    size_t length=1+wcslen(reset)+1,count=1;
    for(const wchar_t *p=source;*p;p+=wcslen(p)+1){length+=wcslen(p)+1;++count;}
    const wchar_t **items=HeapAlloc(GetProcessHeap(),0,count*sizeof(*items));
    if(!items){FreeEnvironmentStringsW(source);return NULL;}
    size_t n=0;items[n++]=reset;
    for(const wchar_t *p=source;*p;p+=wcslen(p)+1)if(_wcsnicmp(p,key,wcslen(key)))items[n++]=p;
    qsort(items,n,sizeof(*items),icons_env_compare);
    wchar_t *block=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,length*sizeof(wchar_t));
    if(block){wchar_t *out=block;for(size_t i=0;i<n;++i){size_t size=wcslen(items[i])+1;memcpy(out,items[i],size*sizeof(wchar_t));out+=size;}*out=0;}
    HeapFree(GetProcessHeap(),0,items);FreeEnvironmentStringsW(source);return block;
}
#endif

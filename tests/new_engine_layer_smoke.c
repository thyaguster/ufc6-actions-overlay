/* SPDX-License-Identifier: GPL-2.0-only
 * Build this fixture as "UFC 6 novo.exe" in a temporary directory beside the
 * bridge and layer. It exercises the actual import/bootstrap/loader chain.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

typedef PFN_vkVoidFunction (VKAPI_PTR *GetInstanceProc)(VkInstance,const char *);

int main(void) {
    MMRESULT timer=timeBeginPeriod(1);
    if(timer==TIMERR_NOERROR)timeEndPeriod(1);
    if(timer!=TIMERR_NOERROR){puts("WINMM forwarding failed");return 1;}
    char layers[1024]={0};
    if(!GetEnvironmentVariableA("VK_INSTANCE_LAYERS",layers,sizeof(layers))||
       !strstr(layers,"VK_LAYER_UFC6_quit_menu")){puts("Vulkan bootstrap missing");return 1;}
    HMODULE loader=LoadLibraryW(L"vulkan-1.dll");
    if(!loader){puts("Vulkan loader unavailable");return 1;}
    GetInstanceProc gipa=(GetInstanceProc)GetProcAddress(loader,"vkGetInstanceProcAddr");
    if(!gipa)return 1;
    PFN_vkCreateInstance create=(PFN_vkCreateInstance)gipa(VK_NULL_HANDLE,"vkCreateInstance");
    if(!create)return 1;
    VkApplicationInfo app={0};app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="UFC 6 new-engine layer smoke";app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo info={0};info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;info.pApplicationInfo=&app;
    VkInstance instance=VK_NULL_HANDLE;
    VkResult result=create(&info,NULL,&instance);
    if(result!=VK_SUCCESS){printf("vkCreateInstance failed: %d\n",result);return 1;}
    PFN_vkDestroyInstance destroy=(PFN_vkDestroyInstance)gipa(instance,"vkDestroyInstance");
    if(destroy)destroy(instance,NULL);
    puts("PASS: new-engine WINMM bootstrap reached the Vulkan layer.");
    return 0;
}

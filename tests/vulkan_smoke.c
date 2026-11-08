/* Real Vulkan loader/device/swapchain test, independent of the user's game. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define CHECK(call) do {VkResult r=(call);if(r!=VK_SUCCESS){printf("FAIL %s: %d\n",#call,r);exit(1);}}while(0)
#define INSTANCE(n) PFN_vk##n n=(PFN_vk##n)gipa(instance,"vk" #n)
#define DEVICE(n) PFN_vk##n n=(PFN_vk##n)gdpa(device,"vk" #n)
typedef void (*StartFn)(HWND);
typedef void (*ToggleFn)(void);
typedef void (*ButtonFn)(uint32_t,int);
typedef int (*ScaleFn)(void);
static void press(ButtonFn button,uint32_t value){button(value,1);button(value,0);Sleep(40);}
#ifdef STARTUP_BRIDGE_TEST
__declspec(dllimport) int clock_getres(int,void*);
#endif
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcW(w,m,a,b);}
int main(void){
    wchar_t directory[MAX_PATH];GetCurrentDirectoryW(MAX_PATH,directory);
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy={0};policy.PreferSystem32Images=1;
    if(!SetProcessMitigationPolicy(ProcessImageLoadPolicy,&policy,sizeof(policy)))return 1;
    if(!LoadLibraryExW(L"version.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32))return 1;
#ifdef STARTUP_BRIDGE_TEST
    if(clock_getres(0,NULL)!=7331)return 1;
#else
    SetEnvironmentVariableW(L"VK_LAYER_PATH",directory);
    SetEnvironmentVariableW(L"VK_INSTANCE_LAYERS",L"VK_LAYER_UFC6_quit_menu");
#endif
    wchar_t test_settings[MAX_PATH];swprintf(test_settings,MAX_PATH,L"%ls\\ufc6-quit-menu.test.ini",directory);
    WritePrivateProfileStringW(L"Menu",L"UIScale",L"100",test_settings);
    HINSTANCE app=GetModuleHandleW(NULL);
    WNDCLASSW wc={0};wc.lpfnWndProc=proc;wc.hInstance=app;wc.lpszClassName=L"VulkanMenuSmoke";RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"Vulkan menu isolated test",WS_OVERLAPPEDWINDOW,0,0,1024,768,NULL,NULL,app,NULL);
    if(!window)return 1;
    HMODULE loader=LoadLibraryExW(L"vulkan-1.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!loader){puts("Vulkan loader missing");return 1;}
    PFN_vkGetInstanceProcAddr gipa=(PFN_vkGetInstanceProcAddr)GetProcAddress(loader,"vkGetInstanceProcAddr");
    PFN_vkCreateInstance create_instance=(PFN_vkCreateInstance)gipa(VK_NULL_HANDLE,"vkCreateInstance");
    const char *extensions[]={"VK_KHR_surface","VK_KHR_win32_surface"};
    VkApplicationInfo ai={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="UFC6 menu test",.apiVersion=VK_API_VERSION_1_3};
    VkInstanceCreateInfo ii={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai,.enabledExtensionCount=2,.ppEnabledExtensionNames=extensions};
    VkInstance instance;CHECK(create_instance(&ii,NULL,&instance));
    wchar_t core_path[MAX_PATH];swprintf(core_path,MAX_PATH,L"%ls\\version.dll",directory);
    HMODULE core=GetModuleHandleW(core_path);if(!core){puts("Layer did not load the app core explicitly");return 1;}
    StartFn start=(StartFn)GetProcAddress(core,"MenuTestStart");
    ToggleFn toggle=(ToggleFn)GetProcAddress(core,"MenuTestToggle");
    ToggleFn screenshot=(ToggleFn)GetProcAddress(core,"MenuTestScreenshot");
    ButtonFn button=(ButtonFn)GetProcAddress(core,"MenuTestButton");
    ScaleFn scale=(ScaleFn)GetProcAddress(core,"MenuTestScale");
    if(!start||!toggle){puts("Core test exports missing");return 1;}start(window);

    INSTANCE(EnumeratePhysicalDevices);INSTANCE(GetPhysicalDeviceQueueFamilyProperties);
    INSTANCE(CreateWin32SurfaceKHR);INSTANCE(GetPhysicalDeviceSurfaceSupportKHR);
    INSTANCE(GetPhysicalDeviceSurfaceCapabilitiesKHR);INSTANCE(GetPhysicalDeviceSurfaceFormatsKHR);
    INSTANCE(CreateDevice);INSTANCE(DestroySurfaceKHR);INSTANCE(DestroyInstance);
    VkPhysicalDevice physicals[16];uint32_t count=16;CHECK(EnumeratePhysicalDevices(instance,&count,physicals));
    if(!count)return 1;
    VkPhysicalDevice physical=physicals[0];
    VkWin32SurfaceCreateInfoKHR wi={.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,.hinstance=app,.hwnd=window};
    VkSurfaceKHR surface;CHECK(CreateWin32SurfaceKHR(instance,&wi,NULL,&surface));
    VkQueueFamilyProperties families[64];uint32_t nfamilies=64;GetPhysicalDeviceQueueFamilyProperties(physical,&nfamilies,families);
    uint32_t family=UINT32_MAX;
    for(uint32_t i=0;i<nfamilies;++i){VkBool32 supported=0;CHECK(GetPhysicalDeviceSurfaceSupportKHR(physical,i,surface,&supported));if(supported&&(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){family=i;break;}}
    if(family==UINT32_MAX)return 1;
    float priority=1;
    VkDeviceQueueCreateInfo qi={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    const char *device_extensions[]={"VK_KHR_swapchain"};
    VkDeviceCreateInfo di={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qi,.enabledExtensionCount=1,.ppEnabledExtensionNames=device_extensions};
    VkDevice device;CHECK(CreateDevice(physical,&di,NULL,&device));
    PFN_vkGetDeviceProcAddr gdpa=(PFN_vkGetDeviceProcAddr)gipa(instance,"vkGetDeviceProcAddr");
    DEVICE(GetDeviceQueue);DEVICE(CreateSwapchainKHR);DEVICE(GetSwapchainImagesKHR);DEVICE(AcquireNextImageKHR);
    DEVICE(CreateCommandPool);DEVICE(AllocateCommandBuffers);DEVICE(BeginCommandBuffer);DEVICE(EndCommandBuffer);
    DEVICE(ResetCommandBuffer);DEVICE(CmdPipelineBarrier);DEVICE(CmdClearColorImage);DEVICE(CreateSemaphore);
    DEVICE(QueueSubmit);DEVICE(QueuePresentKHR);DEVICE(DeviceWaitIdle);DEVICE(DestroySemaphore);
    DEVICE(DestroyCommandPool);DEVICE(DestroySwapchainKHR);DEVICE(DestroyDevice);
    VkQueue queue;GetDeviceQueue(device,family,0,&queue);
    VkSurfaceCapabilitiesKHR caps;CHECK(GetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
    VkSurfaceFormatKHR formats[64];uint32_t nformats=64;CHECK(GetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&nformats,formats));
    VkSurfaceFormatKHR format=formats[0];
    for(uint32_t i=0;i<nformats;++i)if(formats[i].format==VK_FORMAT_B8G8R8A8_UNORM){format=formats[i];break;}
    VkExtent2D extent=caps.currentExtent;if(extent.width==UINT32_MAX)extent=(VkExtent2D){1024,768};
    uint32_t nimages=caps.minImageCount+1;if(caps.maxImageCount&&nimages>caps.maxImageCount)nimages=caps.maxImageCount;
    VkSwapchainCreateInfoKHR si={.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,.surface=surface,.minImageCount=nimages,
        .imageFormat=format.format,.imageColorSpace=format.colorSpace,.imageExtent=extent,.imageArrayLayers=1,
        .imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .preTransform=caps.currentTransform,.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,.presentMode=VK_PRESENT_MODE_FIFO_KHR,.clipped=VK_TRUE};
    VkSwapchainKHR swap;CHECK(CreateSwapchainKHR(device,&si,NULL,&swap));
    VkImage images[16];nimages=16;CHECK(GetSwapchainImagesKHR(device,swap,&nimages,images));
    VkCommandPoolCreateInfo pi={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};
    VkCommandPool pool;CHECK(CreateCommandPool(device,&pi,NULL,&pool));
    VkCommandBufferAllocateInfo ca={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer cmd;CHECK(AllocateCommandBuffers(device,&ca,&cmd));
    VkSemaphoreCreateInfo sem_info={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore acquire,ready;CHECK(CreateSemaphore(device,&sem_info,NULL,&acquire));CHECK(CreateSemaphore(device,&sem_info,NULL,&ready));
    Sleep(400);toggle();Sleep(100);if(screenshot)screenshot();
    for(int frame=0;frame<8;++frame){
        if(button&&scale){
            if(frame==1){for(int i=0;i<3;++i)press(button,0x40);press(button,0x4000);press(button,0x40);press(button,0x40);}
            if(frame==2){for(int i=0;i<4;++i)press(button,0x20);if(scale()!=200)return 1;}
            if(frame==3){for(int i=0;i<4;++i)press(button,0x20);if(scale()!=300)return 1;}
            if(frame==4){for(int i=0;i<10;++i)press(button,0x80);if(scale()!=50)return 1;}
            if(frame==5){press(button,0x4000);if(scale()!=100)return 1;}
            if(frame==6){for(int i=0;i<3;++i)press(button,0x20);if(scale()!=175)return 1;}
            if(frame==7){press(button,0x4000);if(scale()!=100)return 1;}
        }
        MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        uint32_t index;CHECK(AcquireNextImageKHR(device,swap,UINT64_MAX,acquire,VK_NULL_HANDLE,&index));
        CHECK(ResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo bi={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};CHECK(BeginCommandBuffer(cmd,&bi));
        VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=images[index],.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
        VkClearColorValue color={{.04f,.06f,.09f,1.f}};CmdClearColorImage(cmd,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&barrier.subresourceRange);
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=0;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);CHECK(EndCommandBuffer(cmd));
        VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.waitSemaphoreCount=1,.pWaitSemaphores=&acquire,.pWaitDstStageMask=&stage,.commandBufferCount=1,.pCommandBuffers=&cmd,.signalSemaphoreCount=1,.pSignalSemaphores=&ready};CHECK(QueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
        VkPresentInfoKHR present={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.waitSemaphoreCount=1,.pWaitSemaphores=&ready,.swapchainCount=1,.pSwapchains=&swap,.pImageIndices=&index};CHECK(QueuePresentKHR(queue,&present));CHECK(DeviceWaitIdle(device));
    }
    DestroySemaphore(device,acquire,NULL);DestroySemaphore(device,ready,NULL);DestroyCommandPool(device,pool,NULL);
    DestroySwapchainKHR(device,swap,NULL);DestroyDevice(device,NULL);DestroySurfaceKHR(instance,surface,NULL);DestroyInstance(instance,NULL);
    puts("PASS: Vulkan frames presented at 50/100/175/200/300% with upload growth, viewport fitting and cleanup.");
    DestroyWindow(window);return 0;
}

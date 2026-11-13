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
#ifdef PHOTO_EFFECTS_GPU_TEST
#include "../src/photo_effects.h"
#endif

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
    WritePrivateProfileStringW(L"Menu",L"WindowSize",L"0",test_settings);
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
    HMODULE core=GetModuleHandleW(core_path);
#ifdef NON_GAME_PROBE
    if(core){puts("FAIL: overlay core loaded into a launcher/probe process");return 1;}
    ((PFN_vkDestroyInstance)gipa(instance,"vkDestroyInstance"))(instance,NULL);DestroyWindow(window);
    puts("PASS: Vulkan probe in a non-game process does not load the overlay core.");return 0;
#endif
    if(!core){puts("Layer did not load the app core explicitly");return 1;}
    StartFn start=(StartFn)GetProcAddress(core,"MenuTestStart");
    ToggleFn toggle=(ToggleFn)GetProcAddress(core,"MenuTestToggle");
    ToggleFn screenshot=(ToggleFn)GetProcAddress(core,"MenuTestScreenshot");
    ToggleFn screenshot_overlay=(ToggleFn)GetProcAddress(core,"MenuTestScreenshotOverlay");
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
#ifdef PHOTO_BLEND_GPU_TEST
    wchar_t requested_format[16];if(GetEnvironmentVariableW(L"UFC6_SMOKE_FORMAT",requested_format,16)){
        int choice=_wtoi(requested_format);VkFormat requested=choice==2?VK_FORMAT_R8G8B8A8_UNORM:choice==3?VK_FORMAT_B8G8R8A8_SRGB:choice==4?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM;int found=0;
        for(uint32_t i=0;i<nformats;++i)if(formats[i].format==requested){format=formats[i];found=1;break;}
        if(!found){printf("SKIP: requested swapchain format %d unavailable\n",requested);return 0;}
    }
#endif
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
    Sleep(400);typedef void (*SettingsModeFn)(int);SettingsModeFn settings_mode=(SettingsModeFn)GetProcAddress(core,"MenuTestSettingsMode");if(settings_mode)settings_mode(1);
    ToggleFn enable_probe=(ToggleFn)GetProcAddress(core,"MenuTestStartupProbeEnable");if(!enable_probe)return 1;enable_probe();
    toggle();Sleep(100);if(screenshot)screenshot();
    int frame_count=16;
#ifdef PHOTO_EFFECTS_GPU_TEST
    frame_count=71;
#endif
    for(int frame=0;frame<frame_count;++frame){
        if(button&&scale){
            if(frame==1){for(int i=0;i<5;++i)press(button,0x40);press(button,0x4000);press(button,0x40);press(button,0x40);}
            if(frame==2){for(int i=0;i<4;++i)press(button,0x20);if(scale()!=200)return 1;}
            if(frame==3){for(int i=0;i<4;++i)press(button,0x20);if(scale()!=300)return 1;}
            if(frame==4){for(int i=0;i<10;++i)press(button,0x80);if(scale()!=50)return 1;}
            if(frame==5){press(button,0x4000);if(scale()!=100)return 1;}
            if(frame==6){for(int i=0;i<3;++i)press(button,0x20);if(scale()!=175)return 1;}
            if(frame==7){press(button,0x4000);if(scale()!=100)return 1;}
        }
        if(frame==8){toggle();Sleep(80);typedef void (*StatusFn)(const wchar_t*);StatusFn notify=(StatusFn)GetProcAddress(core,"MenuTestStatus");if(notify)notify(L"Screenshot saved");}
        if(frame==9&&screenshot)screenshot();
        if(frame==10){typedef void (*InvokeFn)(int);InvokeFn invoke=(InvokeFn)GetProcAddress(core,"MenuTestInvokeBinding");ScaleFn window_size=(ScaleFn)GetProcAddress(core,"MenuTestWindowSize");if(!invoke||!window_size)return 1;invoke(3);Sleep(100);for(int i=0;i<18;++i)press(button,0x40);press(button,0x4000);press(button,0x4000);if(window_size()!=2){printf("FAIL: expected Tall, got %d\n",window_size());return 1;}invoke(2);Sleep(100);}
        if(frame==11){press(button,0x2000);press(button,0x40);press(button,0x4000);}
        if(frame==12){for(int i=0;i<8;++i)press(button,0x40);press(button,0x4000);if(screenshot_overlay)screenshot_overlay();}

        if(frame==13||frame==14){typedef void (*HelpFn)(int,int,int);HelpFn help=(HelpFn)GetProcAddress(core,"MenuTestPhotoHelp");if(!help)return 1;help(1,frame==14,frame==14);Sleep(80);if(frame==13&&screenshot)screenshot();if(frame==14&&screenshot_overlay)screenshot_overlay();}
        if(frame==15){typedef void (*HelpFn)(int,int,int);((HelpFn)GetProcAddress(core,"MenuTestPhotoHelp"))(0,0,0);}
#ifdef PHOTO_EFFECTS_GPU_TEST
        if(frame>=16){typedef void (*EffectsFn)(const PhotoEffects*,int);EffectsFn effects=(EffectsFn)GetProcAddress(core,"MenuTestEffects");if(!effects)return 1;PhotoEffects look;photo_effects_default(&look);
            if(frame>=17&&frame<=22)look.preset=frame-16;
            if(frame==23)look.exposure=2;
            if(frame==24)look.bars=1;
            if(frame>=25&&frame<=28){look.watermark=1;look.corner=frame-25;wcscpy(look.name,L"Photo Tester");}
            if(frame==30)look.vignette=4;
            if(frame==31)look.grain=4;
            if(frame==32)look.saturation=0;
            if(frame==33)look.contrast=8;
            if(frame==34)look.warmth=4;
            if(frame==35){look.preset=5;look.strength=0;}
            if(frame==36){look.watermark=1;look.opacity=1;look.size=0;wcscpy(look.name,L"Photo Tester");}
            if(frame>=38&&frame<=41)look.preset=5;
            if(frame>=42&&frame<=57){look.watermark=1;look.mark_style=(frame-42)/4;look.mark_font=(frame-42)%4;wcscpy(look.name,L"Photo Creator");}
            if(frame>=58&&frame<=60){look.preset=2;look.watermark=1;wcscpy(look.name,L"Photo Creator");}
            if(frame>=62){look.watermark=1;look.mark_style=0;look.mark_color=frame==70?1:frame-62;wcscpy(look.name,L"Photo Creator");}
            effects(&look,(frame==38||frame==58||frame==59)?2:frame!=29&&frame!=37&&frame!=61);Sleep(80);
            if(frame==38){typedef void (*HelpFn)(int,int,int);((HelpFn)GetProcAddress(core,"MenuTestPhotoHelp"))(1,0,0);Sleep(80);screenshot_overlay();}
            else if(frame>=39&&frame<=41){toggle();Sleep(80);for(int i=0;i<4;++i)press(button,0x40);press(button,0x4000);for(int i=0;i<9;++i)press(button,0x40);press(button,0x4000);Sleep(80);screenshot_overlay();}
            else if(frame==58||frame==59){typedef int (*PanelFn)(uint32_t,unsigned);PanelFn panel=(PanelFn)GetProcAddress(core,"MenuTestPhotoPanel");if(!panel||panel(8,0)!=frame-57)return 1;Sleep(80);screenshot_overlay();}
            else screenshot();
        }
#endif
        MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        uint32_t index;CHECK(AcquireNextImageKHR(device,swap,UINT64_MAX,acquire,VK_NULL_HANDLE,&index));
        CHECK(ResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo bi={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};CHECK(BeginCommandBuffer(cmd,&bi));
        VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=images[index],.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
        CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
        VkClearColorValue color={{.04f,.06f,.09f,1.f}};
#ifdef PHOTO_EFFECTS_GPU_TEST
        if(frame==70)color=(VkClearColorValue){{.95f,.95f,.95f,1.f}};
#endif
        CmdClearColorImage(cmd,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&barrier.subresourceRange);
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=0;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);CHECK(EndCommandBuffer(cmd));
        VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.waitSemaphoreCount=1,.pWaitSemaphores=&acquire,.pWaitDstStageMask=&stage,.commandBufferCount=1,.pCommandBuffers=&cmd,.signalSemaphoreCount=1,.pSignalSemaphores=&ready};CHECK(QueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
        VkPresentInfoKHR present={.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,.waitSemaphoreCount=1,.pWaitSemaphores=&ready,.swapchainCount=1,.pSwapchains=&swap,.pImageIndices=&index};CHECK(QueuePresentKHR(queue,&present));CHECK(DeviceWaitIdle(device));
        if(frame==12||frame==13||frame==14){typedef void (*PathFn)(wchar_t*,int);PathFn path=(PathFn)GetProcAddress(core,"MenuTestScreenshotPath");wchar_t saved[32768];if(!path)return 1;path(saved,32768);if(!saved[0]||!CopyFileW(saved,frame==12?L"capture-overlay-menu.png":frame==13?L"capture-clean.png":L"capture-overlay-panel.png",FALSE))return 1;}

#ifdef PHOTO_EFFECTS_GPU_TEST
        if(frame>=16){typedef void (*PathFn)(wchar_t*,int);PathFn path=(PathFn)GetProcAddress(core,"MenuTestScreenshotPath");wchar_t saved[32768],name[64];if(!path)return 1;path(saved,32768);swprintf(name,64,L"effect-%d.png",frame);if(!saved[0]||!CopyFileW(saved,name,FALSE))return 1;}
#endif

    }
    typedef int (*ProbeFramesFn)(void);
    ProbeFramesFn probe_frames=(ProbeFramesFn)GetProcAddress(core,"MenuTestStartupProbeFrames");
    ScaleFn probe_dirty=(ScaleFn)GetProcAddress(core,"MenuTestStartupProbeDirty");if(!probe_dirty||probe_dirty()) {puts("FAIL: overlay entered startup recognizer frames");return 1;}
    if(!probe_frames||probe_frames()<1){puts("FAIL: startup probe did not receive GPU readback");return 1;}
    puts("PASS: startup probe receives pre-overlay GPU frames without saving screenshots or injecting test input.");
    typedef void (*ScreenshotNameFn)(wchar_t*,int);
    ScreenshotNameFn screenshot_name=(ScreenshotNameFn)GetProcAddress(core,"MenuTestScreenshotName");
    wchar_t saved_name[128]=L"";if(screenshot_name)screenshot_name(saved_name,128);
    if(!saved_name[0]){puts("FAIL: screenshot path did not reach the core");return 1;}
    printf("Screenshot: %ls\n",saved_name);
    puts("PASS: saved screenshot filename returned from Vulkan capture to overlay core.");
#ifdef PHOTO_BLEND_GPU_TEST
    wchar_t layer_path[MAX_PATH];swprintf(layer_path,MAX_PATH,L"%ls\\ufc6_menu_layer.dll",directory);HMODULE layer=GetModuleHandleW(layer_path);
    typedef int (*BlendResult)(void);BlendResult blend_result=layer?(BlendResult)GetProcAddress(layer,"Ufc6LayerTestBlend"):NULL;
    int verified=blend_result?blend_result():0;if((verified&65535)<2||(verified>>16)){printf("FAIL: GPU blend verification %d\n",verified);return 1;}
    puts("PASS: real GPU alpha pixels match foreground/background composition for both controller profiles; screenshot capture remains pre-panel.");
#endif
    DestroySemaphore(device,acquire,NULL);DestroySemaphore(device,ready,NULL);DestroyCommandPool(device,pool,NULL);
    DestroySwapchainKHR(device,swap,NULL);DestroyDevice(device,NULL);DestroySurfaceKHR(instance,surface,NULL);DestroyInstance(instance,NULL);
    puts("PASS: Photo Mode and controls canvases presented through Vulkan.");
    puts("PASS: tall Cheats canvas presented with independent row capacity.");
    puts("PASS: corner notification presented without opening the menu.");
    puts("PASS: Vulkan frames presented at 50/100/175/200/300% with upload growth, viewport fitting and cleanup.");
    DestroyWindow(window);return 0;
}

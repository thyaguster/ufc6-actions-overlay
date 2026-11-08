/* Draw the menu into the presented swapchain image. No desktop popup is involved. */
#define WIN32_LEAN_AND_MEAN
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <stdint.h>
#include <string.h>
#include "screenshot_png.h"
#include "overlay_layout.h"

#define MENU_W 570
#define MENU_H 372
#define MENU_BYTES (MENU_W * MENU_H * 4)
#define MAX_IMAGES 16
#define MAX_SWAPS 4
#define MAX_DEVICES 4
typedef BOOL (*RenderFn)(void *,uint32_t);
typedef void (*ExtentFn)(uint32_t,uint32_t,uint32_t *,uint32_t *);
typedef void (*ActiveFn)(void);
typedef BOOL (*ScreenshotRequestFn)(void);
typedef void (*ScreenshotResultFn)(BOOL);
typedef void (*ScreenshotCapabilityFn)(BOOL);
static HINSTANCE self;
static SRWLOCK guard=SRWLOCK_INIT;
static RenderFn render_menu;
static ExtentFn menu_extent;
static ActiveFn mark_active;
static ScreenshotRequestFn screenshot_request;
static ScreenshotResultFn screenshot_result;
static ScreenshotCapabilityFn screenshot_capability;
static VkInstance instance;
static PFN_vkGetInstanceProcAddr next_gipa;
static PFN_GetPhysicalDeviceProcAddr next_phys;

#define DEVICE_PROCS(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(GetDeviceQueue2) X(CreateSwapchainKHR) \
 X(DestroySwapchainKHR) X(GetSwapchainImagesKHR) X(QueuePresentKHR) X(QueueSubmit) \
 X(DeviceWaitIdle) X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) \
 X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdPipelineBarrier) \
 X(CmdCopyBufferToImage) X(CmdCopyImageToBuffer) X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) \
 X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) \
 X(FlushMappedMemoryRanges) X(InvalidateMappedMemoryRanges) X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) \
 X(CreateSemaphore) X(DestroySemaphore)
typedef struct {
    VkCommandBuffer command;
    VkFence fence;
    VkSemaphore semaphore;
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *mapped;
    int coherent;
    VkDeviceSize buffer_bytes;
} Frame;
typedef struct {
    VkSwapchainKHR swap;
    VkExtent2D extent;
    VkFormat format;
    uint32_t count;
    VkImage images[MAX_IMAGES];
    Frame frames[MAX_IMAGES];
    VkCommandPool pool;
    uint32_t family;
    int supported, ready, failed, logged,capture_supported;
} Swap;
typedef struct {
    VkDevice device;
    VkPhysicalDevice physical;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR surface_caps;
    void *key;
    PFN_vkGetDeviceProcAddr gdpa;
    PFN_vkSetDeviceLoaderData set_data;
    VkPhysicalDeviceMemoryProperties memory;
    struct {VkQueue queue;uint32_t family;} queues[16];
    uint32_t queue_count;
    Swap swaps[MAX_SWAPS];
#define FIELD(n) PFN_vk##n n;
    DEVICE_PROCS(FIELD)
#undef FIELD
} Device;
static Device devices[MAX_DEVICES];

static void log_line(const char *line) {
    wchar_t path[MAX_PATH];GetModuleFileNameW(self,path,MAX_PATH);
    wchar_t *s=wcsrchr(path,L'\\');if(!s)return;lstrcpyW(s+1,L"ufc6-quit-menu-vulkan.log");
    HANDLE h=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,0,NULL);
    if(h!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(h,line,(DWORD)strlen(line),&n,NULL);WriteFile(h,"\r\n",2,&n,NULL);CloseHandle(h);}
}
static Device *device_for(void *handle) {
    if(!handle)return NULL;
    void *key=*(void **)handle;
    for(int i=0;i<MAX_DEVICES;++i)if(devices[i].device&&devices[i].key==key)return &devices[i];
    return NULL;
}
static void bind_menu(void) {
    if(render_menu)return;
    wchar_t path[MAX_PATH];GetModuleFileNameW(self,path,MAX_PATH);
    wchar_t *s=wcsrchr(path,L'\\');if(!s)return;
    lstrcpyW(s+1,L"UFC 6.exe");
    wchar_t executable[MAX_PATH];if(!GetModuleFileNameW(NULL,executable,MAX_PATH)||lstrcmpiW(path,executable))return;
    /* A launcher can probe Vulkan with inherited layer settings. Never load
     * controller hooks into that launcher or any other process. */
    lstrcpyW(s+1,L"version.dll");
    HMODULE core=GetModuleHandleW(path);
    if(!core)core=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!core)return;
    render_menu=(RenderFn)GetProcAddress(core,"Ufc6RenderMenu");
    menu_extent=(ExtentFn)GetProcAddress(core,"Ufc6MenuExtent");
    mark_active=(ActiveFn)GetProcAddress(core,"Ufc6VulkanActive");
    screenshot_request=(ScreenshotRequestFn)GetProcAddress(core,"Ufc6ScreenshotRequest");
    screenshot_result=(ScreenshotResultFn)GetProcAddress(core,"Ufc6ScreenshotResult");
    screenshot_capability=(ScreenshotCapabilityFn)GetProcAddress(core,"Ufc6ScreenshotCapability");
}
typedef struct {VkBuffer buffer;VkDeviceMemory memory;void *mapped;int coherent;} Capture;
static void free_capture(Device *d,Capture *capture) {
    if(capture->mapped)d->UnmapMemory(d->device,capture->memory);
    if(capture->buffer)d->DestroyBuffer(d->device,capture->buffer,NULL);
    if(capture->memory)d->FreeMemory(d->device,capture->memory,NULL);
    memset(capture,0,sizeof(*capture));
}
static int allocate_capture(Device *d,Swap *s,Capture *capture) {
    uint64_t size=(uint64_t)s->extent.width*s->extent.height*4;
    if(!size||size>UINT32_MAX)return 0;
    VkBufferCreateInfo bi={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=size,
        .usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    if(d->CreateBuffer(d->device,&bi,NULL,&capture->buffer)!=VK_SUCCESS)return 0;
    VkMemoryRequirements req;d->GetBufferMemoryRequirements(d->device,capture->buffer,&req);
    uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<d->memory.memoryTypeCount;++i)
        if((req.memoryTypeBits&(1u<<i))&&(d->memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){
            type=i;if((d->memory.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_CACHED_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
                (VK_MEMORY_PROPERTY_HOST_CACHED_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))break;
        }
    if(type==UINT32_MAX)return 0;
    capture->coherent=(d->memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    VkMemoryAllocateInfo mi={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
    return d->AllocateMemory(d->device,&mi,NULL,&capture->memory)==VK_SUCCESS&&
        d->BindBufferMemory(d->device,capture->buffer,capture->memory,0)==VK_SUCCESS&&
        d->MapMemory(d->device,capture->memory,0,VK_WHOLE_SIZE,0,&capture->mapped)==VK_SUCCESS;
}
static void cleanup_swap(Device *d,Swap *s) {
    for(uint32_t i=0;i<s->count;++i){
        Frame *f=&s->frames[i];
        if(f->mapped)d->UnmapMemory(d->device,f->memory);
        if(f->buffer)d->DestroyBuffer(d->device,f->buffer,NULL);
        if(f->memory)d->FreeMemory(d->device,f->memory,NULL);
        if(f->fence)d->DestroyFence(d->device,f->fence,NULL);
        if(f->semaphore)d->DestroySemaphore(d->device,f->semaphore,NULL);
    }
    if(s->pool)d->DestroyCommandPool(d->device,s->pool,NULL);
    memset(s->frames,0,sizeof(s->frames));s->pool=VK_NULL_HANDLE;s->ready=0;
}
static int ensure_upload(Device *d,Frame *f,VkDeviceSize size) {
    if(size<=f->buffer_bytes)return 1;
    VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void *mapped=NULL;
    VkBufferCreateInfo bi={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=size,
        .usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    if(d->CreateBuffer(d->device,&bi,NULL,&buffer)!=VK_SUCCESS)return 0;
    VkMemoryRequirements req;d->GetBufferMemoryRequirements(d->device,buffer,&req);
    uint32_t type=UINT32_MAX;
    for(uint32_t t=0;t<d->memory.memoryTypeCount;++t)
        if((req.memoryTypeBits&(1u<<t))&&(d->memory.memoryTypes[t].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){
            type=t;if(d->memory.memoryTypes[t].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)break;
        }
    if(type==UINT32_MAX)goto fail;
    VkMemoryAllocateInfo mi={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
    if(d->AllocateMemory(d->device,&mi,NULL,&memory)!=VK_SUCCESS||
        d->BindBufferMemory(d->device,buffer,memory,0)!=VK_SUCCESS||
        d->MapMemory(d->device,memory,0,VK_WHOLE_SIZE,0,&mapped)!=VK_SUCCESS)goto fail;
    if(f->mapped)d->UnmapMemory(d->device,f->memory);
    if(f->buffer)d->DestroyBuffer(d->device,f->buffer,NULL);
    if(f->memory)d->FreeMemory(d->device,f->memory,NULL);
    f->buffer=buffer;f->memory=memory;f->mapped=mapped;f->buffer_bytes=size;
    f->coherent=(d->memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    return 1;
fail:
    if(mapped)d->UnmapMemory(d->device,memory);
    if(buffer)d->DestroyBuffer(d->device,buffer,NULL);
    if(memory)d->FreeMemory(d->device,memory,NULL);
    return 0;
}
static int initialize_swap(Device *d,Swap *s,uint32_t family) {
    s->family=family;
    VkCommandPoolCreateInfo pi={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};
    if(d->CreateCommandPool(d->device,&pi,NULL,&s->pool)!=VK_SUCCESS)goto fail;
    VkCommandBuffer commands[MAX_IMAGES];
    VkCommandBufferAllocateInfo ai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=s->pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=s->count};
    if(d->AllocateCommandBuffers(d->device,&ai,commands)!=VK_SUCCESS)goto fail;
    for(uint32_t i=0;i<s->count;++i){
        Frame *f=&s->frames[i];f->command=commands[i];
        if(d->set_data&&d->set_data(d->device,f->command)!=VK_SUCCESS)goto fail;
        VkFenceCreateInfo fi={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,.flags=VK_FENCE_CREATE_SIGNALED_BIT};
        VkSemaphoreCreateInfo si={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if(d->CreateFence(d->device,&fi,NULL,&f->fence)!=VK_SUCCESS||
            d->CreateSemaphore(d->device,&si,NULL,&f->semaphore)!=VK_SUCCESS)goto fail;
    }
    s->ready=1;log_line("Swapchain menu resources ready.");return 1;
fail:
    cleanup_swap(d,s);s->failed=1;log_line("Unable to initialize swapchain menu resources.");return 0;
}
static VkResult VKAPI_CALL menu_present(VkQueue queue,const VkPresentInfoKHR *info) {
    Device *d=device_for(queue);if(!d)return VK_ERROR_INITIALIZATION_FAILED;
    bind_menu();
    if(!render_menu||info->swapchainCount!=1)return d->QueuePresentKHR(queue,info);
    AcquireSRWLockExclusive(&guard);
    Swap *s=NULL;for(int i=0;i<MAX_SWAPS;++i)if(d->swaps[i].swap==info->pSwapchains[0])s=&d->swaps[i];
    uint32_t index=info->pImageIndices[0],family=UINT32_MAX;
    for(uint32_t i=0;i<d->queue_count;++i)if(d->queues[i].queue==queue)family=d->queues[i].family;
    int capture=screenshot_request&&screenshot_request();
    uint32_t menu_width=MENU_W,menu_height=MENU_H;
    if(s&&menu_extent)menu_extent(s->extent.width,s->extent.height,&menu_width,&menu_height);
    uint32_t menu_bytes=menu_width*menu_height*4;
    int draw=s&&menu_width&&menu_height&&menu_width<=s->extent.width&&menu_height<=s->extent.height&&render_menu(NULL,0);
    if(!s||!s->supported||s->failed||index>=s->count||family==UINT32_MAX||(!draw&&!capture)){
        if(capture&&screenshot_result)screenshot_result(FALSE);
        ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);
    }
    if(!s->ready&&!initialize_swap(d,s,family)){
        if(capture&&screenshot_result)screenshot_result(FALSE);
        ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);
    }
    Frame *f=&s->frames[index];
    if(d->WaitForFences(d->device,1,&f->fence,VK_TRUE,UINT64_MAX)!=VK_SUCCESS){
        if(capture&&screenshot_result)screenshot_result(FALSE);
        ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);
    }
    draw=draw&&ensure_upload(d,f,menu_bytes)&&render_menu(f->mapped,menu_bytes);
    Capture shot={0};
    if(capture&&(!s->capture_supported||!allocate_capture(d,s,&shot))){
        free_capture(d,&shot);capture=0;if(screenshot_result)screenshot_result(FALSE);
    }
    if(!draw&&!capture){ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);}
    if(draw&&(s->format==VK_FORMAT_R8G8B8A8_UNORM||s->format==VK_FORMAT_R8G8B8A8_SRGB)){
        unsigned char *p=f->mapped;for(size_t i=0;i<menu_bytes;i+=4){unsigned char b=p[i];p[i]=p[i+2];p[i+2]=b;}
    }
    if(draw&&!f->coherent){VkMappedMemoryRange range={.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory=f->memory,.size=VK_WHOLE_SIZE};d->FlushMappedMemoryRanges(d->device,1,&range);}
    d->ResetCommandBuffer(f->command,0);
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VkResult result=d->BeginCommandBuffer(f->command,&begin);
    if(result!=VK_SUCCESS){s->failed=1;free_capture(d,&shot);if(capture&&screenshot_result)screenshot_result(FALSE);ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);}
    VkImageMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask=capture?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_TRANSFER_WRITE_BIT,.oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout=capture?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=s->images[index],
        .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
    d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
    if(capture){
        VkBufferImageCopy game={.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageExtent={s->extent.width,s->extent.height,1}};
        d->CmdCopyImageToBuffer(f->command,s->images[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,shot.buffer,1,&game);
        VkBufferMemoryBarrier host={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=shot.buffer,.size=VK_WHOLE_SIZE};
        d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
        if(draw){
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);
        }
    }
    VkBufferImageCopy copy={.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
        .imageOffset={(int32_t)(s->extent.width-menu_width)/2,(int32_t)(s->extent.height-menu_height)/2,0},
        .imageExtent={menu_width,menu_height,1}};
    if(draw)d->CmdCopyBufferToImage(f->command,f->buffer,s->images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    barrier.srcAccessMask=draw?VK_ACCESS_TRANSFER_WRITE_BIT:VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=0;
    barrier.oldLayout=draw?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,NULL,0,NULL,1,&barrier);
    result=d->EndCommandBuffer(f->command);
    if(result!=VK_SUCCESS){s->failed=1;free_capture(d,&shot);if(capture&&screenshot_result)screenshot_result(FALSE);ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);}
    VkPipelineStageFlags *stages=NULL;
    if(info->waitSemaphoreCount){
        stages=HeapAlloc(GetProcessHeap(),0,info->waitSemaphoreCount*sizeof(*stages));
        if(!stages){free_capture(d,&shot);if(capture&&screenshot_result)screenshot_result(FALSE);ReleaseSRWLockExclusive(&guard);return d->QueuePresentKHR(queue,info);}
        for(uint32_t i=0;i<info->waitSemaphoreCount;++i)stages[i]=VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.waitSemaphoreCount=info->waitSemaphoreCount,
        .pWaitSemaphores=info->pWaitSemaphores,.pWaitDstStageMask=stages,.commandBufferCount=1,
        .pCommandBuffers=&f->command,.signalSemaphoreCount=1,.pSignalSemaphores=&f->semaphore};
    d->ResetFences(d->device,1,&f->fence);
    result=d->QueueSubmit(queue,1,&submit,f->fence);HeapFree(GetProcessHeap(),0,stages);
    if(result!=VK_SUCCESS){s->failed=1;free_capture(d,&shot);if(capture&&screenshot_result)screenshot_result(FALSE);ReleaseSRWLockExclusive(&guard);return result;}
    if(capture){
        BOOL saved=FALSE;
        if(d->WaitForFences(d->device,1,&f->fence,VK_TRUE,UINT64_MAX)==VK_SUCCESS){
            if(!shot.coherent){VkMappedMemoryRange range={.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,.memory=shot.memory,.size=VK_WHOLE_SIZE};
                d->InvalidateMappedMemoryRanges(d->device,1,&range);}
            int rgba=s->format==VK_FORMAT_R8G8B8A8_UNORM||s->format==VK_FORMAT_R8G8B8A8_SRGB;
            saved=menu_save_png(self,s->extent.width,s->extent.height,shot.mapped,rgba);
        }
        log_line(saved?"Clean pre-overlay game frame saved as PNG.":"PNG screenshot failed.");
        free_capture(d,&shot);if(screenshot_result)screenshot_result(saved);
    }
    VkPresentInfoKHR present=*info;present.waitSemaphoreCount=1;present.pWaitSemaphores=&f->semaphore;
    if(draw&&!s->logged){log_line("Menu drawn directly into a presented game frame.");s->logged=1;}
    result=d->QueuePresentKHR(queue,&present);
    ReleaseSRWLockExclusive(&guard);return result;
}
static VkResult VKAPI_CALL menu_create_swap(VkDevice device,const VkSwapchainCreateInfoKHR *info,const VkAllocationCallbacks *alloc,VkSwapchainKHR *out){
    Device *d=device_for(device);if(!d)return VK_ERROR_INITIALIZATION_FAILED;
    VkSwapchainCreateInfoKHR create=*info;
    VkSurfaceCapabilitiesKHR caps;
    if(d->surface_caps&&d->surface_caps(d->physical,info->surface,&caps)==VK_SUCCESS&&
        (caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)&&!(info->flags&VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR))
        create.imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkResult result=d->CreateSwapchainKHR(device,&create,alloc,out);if(result!=VK_SUCCESS)return result;
    AcquireSRWLockExclusive(&guard);
    for(int i=0;i<MAX_SWAPS;++i)if(!d->swaps[i].swap){
        Swap *s=&d->swaps[i];s->swap=*out;s->extent=info->imageExtent;s->format=info->imageFormat;
        uint32_t count=0;d->GetSwapchainImagesKHR(device,*out,&count,NULL);
        if(count>0&&count<=MAX_IMAGES){s->count=count;d->GetSwapchainImagesKHR(device,*out,&s->count,s->images);}
        s->supported=s->count&&s->extent.width>=320&&s->extent.height>=240&&
            (info->imageUsage&VK_IMAGE_USAGE_TRANSFER_DST_BIT)&&info->imageArrayLayers==1&&
            (s->format==VK_FORMAT_B8G8R8A8_UNORM||s->format==VK_FORMAT_B8G8R8A8_SRGB||
             s->format==VK_FORMAT_R8G8B8A8_UNORM||s->format==VK_FORMAT_R8G8B8A8_SRGB);
        bind_menu();if(s->supported&&mark_active)mark_active();
        s->capture_supported=s->supported&&(create.imageUsage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        if(screenshot_capability)screenshot_capability(s->capture_supported);
        log_line(s->supported?"Supported game swapchain captured.":"Unsupported swapchain format/usage for menu.");break;
    }
    ReleaseSRWLockExclusive(&guard);return result;
}
static void VKAPI_CALL menu_destroy_swap(VkDevice device,VkSwapchainKHR swap,const VkAllocationCallbacks *alloc){
    Device *d=device_for(device);if(!d)return;
    AcquireSRWLockExclusive(&guard);
    for(int i=0;i<MAX_SWAPS;++i)if(d->swaps[i].swap==swap){
        if(d->swaps[i].ready)d->DeviceWaitIdle(device);
        cleanup_swap(d,&d->swaps[i]);memset(&d->swaps[i],0,sizeof(Swap));break;
    }
    ReleaseSRWLockExclusive(&guard);d->DestroySwapchainKHR(device,swap,alloc);
}
static void remember_queue(Device *d,VkQueue queue,uint32_t family){
    if(!queue)return;
    for(uint32_t i=0;i<d->queue_count;++i)if(d->queues[i].queue==queue)return;
    if(d->queue_count<16){uint32_t n=d->queue_count++;d->queues[n].queue=queue;d->queues[n].family=family;}
}
static void VKAPI_CALL menu_get_queue(VkDevice device,uint32_t family,uint32_t index,VkQueue *out){
    Device *d=device_for(device);if(!d)return;d->GetDeviceQueue(device,family,index,out);remember_queue(d,*out,family);
}
static void VKAPI_CALL menu_get_queue2(VkDevice device,const VkDeviceQueueInfo2 *info,VkQueue *out){
    Device *d=device_for(device);if(!d||!d->GetDeviceQueue2)return;d->GetDeviceQueue2(device,info,out);remember_queue(d,*out,info->queueFamilyIndex);
}
static void VKAPI_CALL menu_destroy_device(VkDevice device,const VkAllocationCallbacks *alloc){
    Device *d=device_for(device);if(!d)return;d->DeviceWaitIdle(device);
    for(int i=0;i<MAX_SWAPS;++i)if(d->swaps[i].swap)cleanup_swap(d,&d->swaps[i]);
    PFN_vkDestroyDevice destroy=d->DestroyDevice;memset(d,0,sizeof(*d));destroy(device,alloc);
}
static VkResult VKAPI_CALL menu_create_device(VkPhysicalDevice physical,const VkDeviceCreateInfo *info,const VkAllocationCallbacks *alloc,VkDevice *out){
    VkLayerDeviceCreateInfo *link=(VkLayerDeviceCreateInfo *)info->pNext;
    PFN_vkSetDeviceLoaderData set_data=NULL;
    for(const VkLayerDeviceCreateInfo *p=link;p;p=p->pNext)
        if(p->sType==VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO&&p->function==VK_LOADER_DATA_CALLBACK)set_data=p->u.pfnSetDeviceLoaderData;
    while(link&&(link->sType!=VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO||link->function!=VK_LAYER_LINK_INFO))link=(VkLayerDeviceCreateInfo *)link->pNext;
    if(!link)return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetInstanceProcAddr gipa=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa=link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    PFN_vkCreateDevice create=(PFN_vkCreateDevice)gipa(instance,"vkCreateDevice");
    VkResult result=create(physical,info,alloc,out);if(result!=VK_SUCCESS)return result;
    Device *d=NULL;for(int i=0;i<MAX_DEVICES;++i)if(!devices[i].device){d=&devices[i];break;}
    if(!d){((PFN_vkDestroyDevice)gdpa(*out,"vkDestroyDevice"))(*out,alloc);return VK_ERROR_TOO_MANY_OBJECTS;}
    d->device=*out;d->key=*(void **)*out;d->gdpa=gdpa;d->set_data=set_data;d->physical=physical;
    d->surface_caps=(PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)gipa(instance,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
#define LOAD(n) d->n=(PFN_vk##n)gdpa(*out,"vk" #n);
    DEVICE_PROCS(LOAD)
#undef LOAD
    ((PFN_vkGetPhysicalDeviceMemoryProperties)gipa(instance,"vkGetPhysicalDeviceMemoryProperties"))(physical,&d->memory);
    log_line("Vulkan device dispatch captured.");return result;
}
static VkResult VKAPI_CALL menu_create_instance(const VkInstanceCreateInfo *info,const VkAllocationCallbacks *alloc,VkInstance *out){
    bind_menu(); /* Explicit core path, outside DllMain; system VERSION.dll may already be loaded. */
    VkLayerInstanceCreateInfo *link=(VkLayerInstanceCreateInfo *)info->pNext;
    while(link&&(link->sType!=VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO||link->function!=VK_LAYER_LINK_INFO))link=(VkLayerInstanceCreateInfo *)link->pNext;
    if(!link)return VK_ERROR_INITIALIZATION_FAILED;
    next_gipa=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;next_phys=link->u.pLayerInfo->pfnNextGetPhysicalDeviceProcAddr;
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    PFN_vkCreateInstance create=(PFN_vkCreateInstance)next_gipa(VK_NULL_HANDLE,"vkCreateInstance");
    VkResult result=create(info,alloc,out);if(result==VK_SUCCESS){instance=*out;log_line("Vulkan menu layer attached to game instance.");}return result;
}
static void VKAPI_CALL menu_destroy_instance(VkInstance inst,const VkAllocationCallbacks *alloc){
    ((PFN_vkDestroyInstance)next_gipa(inst,"vkDestroyInstance"))(inst,alloc);instance=VK_NULL_HANDLE;
}
__declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,const char *name){
    if(!strcmp(name,"vkGetDeviceProcAddr"))return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if(!strcmp(name,"vkQueuePresentKHR"))return (PFN_vkVoidFunction)menu_present;
    if(!strcmp(name,"vkCreateSwapchainKHR"))return (PFN_vkVoidFunction)menu_create_swap;
    if(!strcmp(name,"vkDestroySwapchainKHR"))return (PFN_vkVoidFunction)menu_destroy_swap;
    if(!strcmp(name,"vkGetDeviceQueue"))return (PFN_vkVoidFunction)menu_get_queue;
    if(!strcmp(name,"vkGetDeviceQueue2"))return (PFN_vkVoidFunction)menu_get_queue2;
    if(!strcmp(name,"vkDestroyDevice"))return (PFN_vkVoidFunction)menu_destroy_device;
    Device *d=device_for(device);return d?d->gdpa(device,name):NULL;
}
__declspec(dllexport) PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance inst,const char *name){
    if(!strcmp(name,"vkGetInstanceProcAddr"))return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    if(!strcmp(name,"vkGetDeviceProcAddr"))return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if(!strcmp(name,"vkCreateInstance"))return (PFN_vkVoidFunction)menu_create_instance;
    if(!strcmp(name,"vkDestroyInstance"))return (PFN_vkVoidFunction)menu_destroy_instance;
    if(!strcmp(name,"vkCreateDevice"))return (PFN_vkVoidFunction)menu_create_device;
    if(!strcmp(name,"vkQueuePresentKHR"))return (PFN_vkVoidFunction)menu_present;
    if(!strcmp(name,"vkCreateSwapchainKHR"))return (PFN_vkVoidFunction)menu_create_swap;
    if(!strcmp(name,"vkDestroySwapchainKHR"))return (PFN_vkVoidFunction)menu_destroy_swap;
    if(!strcmp(name,"vkGetDeviceQueue"))return (PFN_vkVoidFunction)menu_get_queue;
    if(!strcmp(name,"vkGetDeviceQueue2"))return (PFN_vkVoidFunction)menu_get_queue2;
    if(!strcmp(name,"vkDestroyDevice"))return (PFN_vkVoidFunction)menu_destroy_device;
    return next_gipa?next_gipa(inst,name):NULL;
}
static PFN_vkVoidFunction VKAPI_CALL physical_proc(VkInstance inst,const char *name){return next_phys?next_phys(inst,name):next_gipa(inst,name);}
VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v){
    if(v->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
    v->loaderLayerInterfaceVersion=2;v->pfnGetInstanceProcAddr=vkGetInstanceProcAddr;
    v->pfnGetDeviceProcAddr=vkGetDeviceProcAddr;v->pfnGetPhysicalDeviceProcAddr=physical_proc;return VK_SUCCESS;
}
BOOL WINAPI DllMain(HINSTANCE dll,DWORD reason,LPVOID reserved){(void)reserved;if(reason==DLL_PROCESS_ATTACH){self=dll;DisableThreadLibraryCalls(dll);}return TRUE;}

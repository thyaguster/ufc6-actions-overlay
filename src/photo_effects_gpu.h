#ifndef UFC6_PHOTO_EFFECTS_GPU_H
#define UFC6_PHOTO_EFFECTS_GPU_H
#include "photo_effects_shaders.h"
static void free_effect_buffer(Device *d,EffectBuffer *b){if(b->mapped)d->UnmapMemory(d->device,b->memory);if(b->buffer)d->DestroyBuffer(d->device,b->buffer,NULL);if(b->memory)d->FreeMemory(d->device,b->memory,NULL);memset(b,0,sizeof(*b));}
static int effect_buffer(Device *d,EffectBuffer *b,VkDeviceSize size,int host){
 if(b->buffer)return 1;
 VkBufferCreateInfo create={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=size,.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};
 if(d->CreateBuffer(d->device,&create,NULL,&b->buffer)!=VK_SUCCESS)goto fail;
 VkMemoryRequirements req;d->GetBufferMemoryRequirements(d->device,b->buffer,&req);uint32_t type=UINT32_MAX;
 for(uint32_t i=0;i<d->memory.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(!host||(d->memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))){type=i;if(d->memory.memoryTypes[i].propertyFlags&(host?VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))break;}
 if(type==UINT32_MAX)goto fail;VkMemoryAllocateInfo allocate={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,.memoryTypeIndex=type};
 if(d->AllocateMemory(d->device,&allocate,NULL,&b->memory)!=VK_SUCCESS||d->BindBufferMemory(d->device,b->buffer,b->memory,0)!=VK_SUCCESS)goto fail;
 if(host&&d->MapMemory(d->device,b->memory,0,VK_WHOLE_SIZE,0,&b->mapped)!=VK_SUCCESS)goto fail;
 b->coherent=(d->memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;return 1;
 fail:free_effect_buffer(d,b);return 0;
}
static void cleanup_effects(Device *d,Swap *s){
 if(s->fx_pool)d->DestroyDescriptorPool(d->device,s->fx_pool,NULL);
 if(s->fx_pipeline)d->DestroyPipeline(d->device,s->fx_pipeline,NULL);
 if(s->fx_layout)d->DestroyPipelineLayout(d->device,s->fx_layout,NULL);
 if(s->fx_descriptors)d->DestroyDescriptorSetLayout(d->device,s->fx_descriptors,NULL);
 for(uint32_t i=0;i<s->count;++i){
  if(s->fx_frames[i])d->DestroyFramebuffer(d->device,s->fx_frames[i],NULL);
  if(s->fx_views[i])d->DestroyImageView(d->device,s->fx_views[i],NULL);
  free_effect_buffer(d,&s->frames[i].fx_image);free_effect_buffer(d,&s->frames[i].fx_mark);
  s->fx_frames[i]=VK_NULL_HANDLE;s->fx_views[i]=VK_NULL_HANDLE;s->frames[i].fx_set=VK_NULL_HANDLE;
 }
 if(s->fx_pass)d->DestroyRenderPass(d->device,s->fx_pass,NULL);
 s->fx_pool=VK_NULL_HANDLE;s->fx_pipeline=VK_NULL_HANDLE;s->fx_layout=VK_NULL_HANDLE;s->fx_descriptors=VK_NULL_HANDLE;s->fx_pass=VK_NULL_HANDLE;s->fx_ready=0;
}
static int initialize_effects(Device *d,Swap *s){
 VkShaderModule vertex=VK_NULL_HANDLE,fragment=VK_NULL_HANDLE;
 if(!d->CreateImageView||!d->CreateRenderPass||!d->CreateFramebuffer||!d->CreateGraphicsPipelines||!d->CreateShaderModule||!d->CreatePipelineLayout||!d->CreateDescriptorSetLayout||!d->CreateDescriptorPool||!d->AllocateDescriptorSets||!d->UpdateDescriptorSets)goto fail;
 VkAttachmentDescription attachment={.format=s->format,.samples=VK_SAMPLE_COUNT_1_BIT,.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
 VkAttachmentReference reference={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
 VkSubpassDescription subpass={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&reference};
 VkRenderPassCreateInfo pass={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=1,.pAttachments=&attachment,.subpassCount=1,.pSubpasses=&subpass};
 if(d->CreateRenderPass(d->device,&pass,NULL,&s->fx_pass)!=VK_SUCCESS)goto fail;
 VkDescriptorSetLayoutBinding bindings[2]={{.binding=0,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT},{.binding=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT}};
 VkDescriptorSetLayoutCreateInfo descriptors={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=2,.pBindings=bindings};
 if(d->CreateDescriptorSetLayout(d->device,&descriptors,NULL,&s->fx_descriptors)!=VK_SUCCESS)goto fail;
 VkPushConstantRange constants={VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(PhotoEffectConstants)};
 VkPipelineLayoutCreateInfo layout={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&s->fx_descriptors,.pushConstantRangeCount=1,.pPushConstantRanges=&constants};
 if(d->CreatePipelineLayout(d->device,&layout,NULL,&s->fx_layout)!=VK_SUCCESS)goto fail;
 VkShaderModuleCreateInfo shader={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=sizeof(photo_effects_vert),.pCode=photo_effects_vert};
 if(d->CreateShaderModule(d->device,&shader,NULL,&vertex)!=VK_SUCCESS)goto fail;
 shader.codeSize=sizeof(photo_effects_frag);shader.pCode=photo_effects_frag;
 if(d->CreateShaderModule(d->device,&shader,NULL,&fragment)!=VK_SUCCESS)goto fail;
 VkPipelineShaderStageCreateInfo stages[]={
  {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=vertex,.pName="main"},
  {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=fragment,.pName="main"}};
 VkPipelineVertexInputStateCreateInfo input={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
 VkPipelineInputAssemblyStateCreateInfo assembly={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
 VkPipelineViewportStateCreateInfo viewport={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.scissorCount=1};
 VkPipelineRasterizationStateCreateInfo raster={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1};
 VkPipelineMultisampleStateCreateInfo multisample={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
 VkPipelineColorBlendAttachmentState alpha={.blendEnable=VK_FALSE,.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,.colorBlendOp=VK_BLEND_OP_ADD,.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,.alphaBlendOp=VK_BLEND_OP_ADD,.colorWriteMask=15};
 VkPipelineColorBlendStateCreateInfo fx={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&alpha};
 VkDynamicState dynamic_states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
 VkPipelineDynamicStateCreateInfo dynamic={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=2,.pDynamicStates=dynamic_states};
 VkGraphicsPipelineCreateInfo pipeline={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,.pVertexInputState=&input,.pInputAssemblyState=&assembly,.pViewportState=&viewport,.pRasterizationState=&raster,.pMultisampleState=&multisample,.pColorBlendState=&fx,.pDynamicState=&dynamic,.layout=s->fx_layout,.renderPass=s->fx_pass};
 if(d->CreateGraphicsPipelines(d->device,VK_NULL_HANDLE,1,&pipeline,NULL,&s->fx_pipeline)!=VK_SUCCESS)goto fail;
 d->DestroyShaderModule(d->device,vertex,NULL);vertex=VK_NULL_HANDLE;d->DestroyShaderModule(d->device,fragment,NULL);fragment=VK_NULL_HANDLE;
 VkDescriptorPoolSize pool_size={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,s->count*2};
 VkDescriptorPoolCreateInfo pool={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=s->count,.poolSizeCount=1,.pPoolSizes=&pool_size};
 if(d->CreateDescriptorPool(d->device,&pool,NULL,&s->fx_pool)!=VK_SUCCESS)goto fail;
 for(uint32_t i=0;i<s->count;++i){
  VkImageViewCreateInfo view={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=s->images[i],.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=s->format,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  if(d->CreateImageView(d->device,&view,NULL,&s->fx_views[i])!=VK_SUCCESS)goto fail;
  VkFramebufferCreateInfo frame={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,.renderPass=s->fx_pass,.attachmentCount=1,.pAttachments=&s->fx_views[i],.width=s->extent.width,.height=s->extent.height,.layers=1};
  if(d->CreateFramebuffer(d->device,&frame,NULL,&s->fx_frames[i])!=VK_SUCCESS)goto fail;
  VkDescriptorSetAllocateInfo set={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=s->fx_pool,.descriptorSetCount=1,.pSetLayouts=&s->fx_descriptors};
  if(d->AllocateDescriptorSets(d->device,&set,&s->frames[i].fx_set)!=VK_SUCCESS)goto fail;
 }
 s->fx_ready=1;log_line("Photo Mode GPU filters ready.");return 1;
fail:
 if(vertex)d->DestroyShaderModule(d->device,vertex,NULL);if(fragment)d->DestroyShaderModule(d->device,fragment,NULL);
 cleanup_effects(d,s);s->fx_failed=1;log_line("Photo Mode filters unavailable on this swapchain; original image preserved.");return 0;
}

static int prepare_effects(Device *d,Swap *s,Frame *f,const PhotoEffects *p,PhotoEffectConstants *c){
 VkDeviceSize bytes=(VkDeviceSize)s->extent.width*s->extent.height*4;
 if(bytes>128u*1024*1024||!effect_buffer(d,&f->fx_image,bytes,0)||!effect_buffer(d,&f->fx_mark,PHOTO_MARK_BYTES,1))return 0;
 uint32_t mark_width=0;
 if(p->watermark&&p->name[0]&&photo_mark){unsigned char *raster=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,PHOTO_MARK_BYTES);if(!raster)return 0;mark_width=photo_mark(raster,PHOTO_MARK_BYTES);memcpy(f->fx_mark.mapped,raster,PHOTO_MARK_BYTES);HeapFree(GetProcessHeap(),0,raster);
  if(!f->fx_mark.coherent){VkMappedMemoryRange range={.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,.memory=f->fx_mark.memory,.size=VK_WHOLE_SIZE};if(d->FlushMappedMemoryRanges(d->device,1,&range)!=VK_SUCCESS)return 0;}}
 *c=(PhotoEffectConstants){.width=s->extent.width,.height=s->extent.height,.rgba=s->format==VK_FORMAT_R8G8B8A8_UNORM||s->format==VK_FORMAT_R8G8B8A8_SRGB,.srgb=s->format==VK_FORMAT_R8G8B8A8_SRGB||s->format==VK_FORMAT_B8G8R8A8_SRGB,
 .exposure=powf(2.f,p->exposure*.5f),.contrast=1.f+(p->contrast-4)*.15f,.saturation=p->saturation*.25f,.warmth=p->warmth*.03f,.vignette=p->vignette*.2f,.grain=p->grain*.035f,.bars=p->bars==1?2.35f:p->bars==2?2.76f:0,.preset=(float)p->preset,.strength=p->strength*.25f,
 .corner=(float)p->corner,.opacity=mark_width?p->opacity*.25f:0,.size=p->size==0?.65f:p->size==2?1.35f:1.f,.mark_width=mark_width,.mark_height=PHOTO_MARK_H,.reserved={PHOTO_MARK_W,(photo_mark_rgb(p)<<8)|(uint32_t)p->mark_style}};
 VkDescriptorBufferInfo info[2]={{f->fx_image.buffer,0,bytes},{f->fx_mark.buffer,0,PHOTO_MARK_BYTES}};VkWriteDescriptorSet writes[2];
 for(int i=0;i<2;++i)writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->fx_set,.dstBinding=(uint32_t)i,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&info[i]};d->UpdateDescriptorSets(d->device,2,writes,0,NULL);return 1;
}
static VkAccessFlags image_access(VkImageLayout layout){return layout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL?VK_ACCESS_TRANSFER_READ_BIT:layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL?VK_ACCESS_TRANSFER_WRITE_BIT:layout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL?(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT):0;}
static VkPipelineStageFlags image_stage(VkImageLayout layout){return layout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL?VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:layout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR?VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT;}
static void image_transition(Device *d,Swap *s,Frame *f,uint32_t index,VkImageLayout before,VkImageLayout after){
 VkImageMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.srcAccessMask=image_access(before),.dstAccessMask=image_access(after),.oldLayout=before,.newLayout=after,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=s->images[index],.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
 d->CmdPipelineBarrier(f->command,image_stage(before),image_stage(after),0,0,NULL,0,NULL,1,&b);
}
static void record_readback(Device *d,Swap *s,Frame *f,uint32_t index,VkBuffer buffer){
 VkBufferImageCopy copy={.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageExtent={s->extent.width,s->extent.height,1}};d->CmdCopyImageToBuffer(f->command,s->images[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
 VkBufferMemoryBarrier host={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=buffer,.size=VK_WHOLE_SIZE};d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&host,0,NULL);
}
static void draw_effects(Device *d,Swap *s,Frame *f,uint32_t index,const PhotoEffectConstants *c){
 VkBufferImageCopy copy={.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},.imageExtent={s->extent.width,s->extent.height,1}};
 d->CmdCopyImageToBuffer(f->command,s->images[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,f->fx_image.buffer,1,&copy);
 VkBufferMemoryBarrier buffers[2]={{.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=f->fx_image.buffer,.size=VK_WHOLE_SIZE},{.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=f->fx_mark.buffer,.size=VK_WHOLE_SIZE}};
 VkImageMemoryBarrier image={.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT,.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=s->images[index],.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
 d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,NULL,2,buffers,1,&image);
 VkRenderPassBeginInfo begin={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,.renderPass=s->fx_pass,.framebuffer=s->fx_frames[index],.renderArea={{0,0},s->extent}};
 d->CmdBeginRenderPass(f->command,&begin,VK_SUBPASS_CONTENTS_INLINE);d->CmdBindPipeline(f->command,VK_PIPELINE_BIND_POINT_GRAPHICS,s->fx_pipeline);
 VkViewport viewport={0,0,(float)s->extent.width,(float)s->extent.height,0,1};VkRect2D scissor={{0,0},s->extent};d->CmdSetViewport(f->command,0,1,&viewport);d->CmdSetScissor(f->command,0,1,&scissor);
 d->CmdBindDescriptorSets(f->command,VK_PIPELINE_BIND_POINT_GRAPHICS,s->fx_layout,0,1,&f->fx_set,0,NULL);d->CmdPushConstants(f->command,s->fx_layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(*c),c);d->CmdDraw(f->command,3,1,0,0);d->CmdEndRenderPass(f->command);
}
#endif

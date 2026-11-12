#ifndef UFC6_PHOTO_BLEND_H
#define UFC6_PHOTO_BLEND_H
#include "photo_blend_shaders.h"
static void cleanup_blend(Device *d,Swap *s){
 if(s->blend_pool)d->DestroyDescriptorPool(d->device,s->blend_pool,NULL);
 if(s->blend_pipeline)d->DestroyPipeline(d->device,s->blend_pipeline,NULL);
 if(s->blend_layout)d->DestroyPipelineLayout(d->device,s->blend_layout,NULL);
 if(s->blend_descriptors)d->DestroyDescriptorSetLayout(d->device,s->blend_descriptors,NULL);
 for(uint32_t i=0;i<s->count;++i){
  if(s->blend_frames[i])d->DestroyFramebuffer(d->device,s->blend_frames[i],NULL);
  if(s->blend_views[i])d->DestroyImageView(d->device,s->blend_views[i],NULL);
  s->blend_frames[i]=VK_NULL_HANDLE;s->blend_views[i]=VK_NULL_HANDLE;s->frames[i].blend_set=VK_NULL_HANDLE;
 }
 if(s->blend_pass)d->DestroyRenderPass(d->device,s->blend_pass,NULL);
 s->blend_pool=VK_NULL_HANDLE;s->blend_pipeline=VK_NULL_HANDLE;s->blend_layout=VK_NULL_HANDLE;s->blend_descriptors=VK_NULL_HANDLE;s->blend_pass=VK_NULL_HANDLE;s->blend_ready=0;
}
static int initialize_blend(Device *d,Swap *s){
 VkShaderModule vertex=VK_NULL_HANDLE,fragment=VK_NULL_HANDLE;
 if(!d->CreateImageView||!d->CreateRenderPass||!d->CreateFramebuffer||!d->CreateGraphicsPipelines||!d->CreateShaderModule||!d->CreatePipelineLayout||!d->CreateDescriptorSetLayout||!d->CreateDescriptorPool||!d->AllocateDescriptorSets||!d->UpdateDescriptorSets)goto fail;
 VkAttachmentDescription attachment={.format=s->format,.samples=VK_SAMPLE_COUNT_1_BIT,.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp=VK_ATTACHMENT_STORE_OP_STORE,.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
 VkAttachmentReference reference={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
 VkSubpassDescription subpass={.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,.colorAttachmentCount=1,.pColorAttachments=&reference};
 VkRenderPassCreateInfo pass={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,.attachmentCount=1,.pAttachments=&attachment,.subpassCount=1,.pSubpasses=&subpass};
 if(d->CreateRenderPass(d->device,&pass,NULL,&s->blend_pass)!=VK_SUCCESS)goto fail;
 VkDescriptorSetLayoutBinding binding={.binding=0,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT};
 VkDescriptorSetLayoutCreateInfo descriptors={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=1,.pBindings=&binding};
 if(d->CreateDescriptorSetLayout(d->device,&descriptors,NULL,&s->blend_descriptors)!=VK_SUCCESS)goto fail;
 VkPushConstantRange constants={VK_SHADER_STAGE_FRAGMENT_BIT,0,12};
 VkPipelineLayoutCreateInfo layout={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&s->blend_descriptors,.pushConstantRangeCount=1,.pPushConstantRanges=&constants};
 if(d->CreatePipelineLayout(d->device,&layout,NULL,&s->blend_layout)!=VK_SUCCESS)goto fail;
 VkShaderModuleCreateInfo shader={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=sizeof(photo_blend_vert),.pCode=photo_blend_vert};
 if(d->CreateShaderModule(d->device,&shader,NULL,&vertex)!=VK_SUCCESS)goto fail;
 shader.codeSize=sizeof(photo_blend_frag);shader.pCode=photo_blend_frag;
 if(d->CreateShaderModule(d->device,&shader,NULL,&fragment)!=VK_SUCCESS)goto fail;
 VkPipelineShaderStageCreateInfo stages[]={
  {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=vertex,.pName="main"},
  {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=fragment,.pName="main"}};
 VkVertexInputBindingDescription vb={0,8,VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription va={0,0,VK_FORMAT_R32G32_SFLOAT,0};
 VkPipelineVertexInputStateCreateInfo input={.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,.vertexBindingDescriptionCount=1,.pVertexBindingDescriptions=&vb,.vertexAttributeDescriptionCount=1,.pVertexAttributeDescriptions=&va};
 VkPipelineInputAssemblyStateCreateInfo assembly={.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
 VkPipelineViewportStateCreateInfo viewport={.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.scissorCount=1};
 VkPipelineRasterizationStateCreateInfo raster={.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1};
 VkPipelineMultisampleStateCreateInfo multisample={.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
 VkPipelineColorBlendAttachmentState alpha={.blendEnable=VK_TRUE,.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,.colorBlendOp=VK_BLEND_OP_ADD,.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,.alphaBlendOp=VK_BLEND_OP_ADD,.colorWriteMask=15};
 VkPipelineColorBlendStateCreateInfo blend={.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=1,.pAttachments=&alpha};
 VkDynamicState dynamic_states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
 VkPipelineDynamicStateCreateInfo dynamic={.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=2,.pDynamicStates=dynamic_states};
 VkGraphicsPipelineCreateInfo pipeline={.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.stageCount=2,.pStages=stages,.pVertexInputState=&input,.pInputAssemblyState=&assembly,.pViewportState=&viewport,.pRasterizationState=&raster,.pMultisampleState=&multisample,.pColorBlendState=&blend,.pDynamicState=&dynamic,.layout=s->blend_layout,.renderPass=s->blend_pass};
 if(d->CreateGraphicsPipelines(d->device,VK_NULL_HANDLE,1,&pipeline,NULL,&s->blend_pipeline)!=VK_SUCCESS)goto fail;
 d->DestroyShaderModule(d->device,vertex,NULL);vertex=VK_NULL_HANDLE;d->DestroyShaderModule(d->device,fragment,NULL);fragment=VK_NULL_HANDLE;
 VkDescriptorPoolSize pool_size={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,s->count};
 VkDescriptorPoolCreateInfo pool={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=s->count,.poolSizeCount=1,.pPoolSizes=&pool_size};
 if(d->CreateDescriptorPool(d->device,&pool,NULL,&s->blend_pool)!=VK_SUCCESS)goto fail;
 for(uint32_t i=0;i<s->count;++i){
  VkImageViewCreateInfo view={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=s->images[i],.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=s->format,.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
  if(d->CreateImageView(d->device,&view,NULL,&s->blend_views[i])!=VK_SUCCESS)goto fail;
  VkFramebufferCreateInfo frame={.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,.renderPass=s->blend_pass,.attachmentCount=1,.pAttachments=&s->blend_views[i],.width=s->extent.width,.height=s->extent.height,.layers=1};
  if(d->CreateFramebuffer(d->device,&frame,NULL,&s->blend_frames[i])!=VK_SUCCESS)goto fail;
  VkDescriptorSetAllocateInfo set={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=s->blend_pool,.descriptorSetCount=1,.pSetLayouts=&s->blend_descriptors};
  if(d->AllocateDescriptorSets(d->device,&set,&s->frames[i].blend_set)!=VK_SUCCESS)goto fail;
 }
 s->blend_ready=1;log_line("Photo Mode GPU alpha blending ready.");return 1;
fail:
 if(vertex)d->DestroyShaderModule(d->device,vertex,NULL);if(fragment)d->DestroyShaderModule(d->device,fragment,NULL);
 cleanup_blend(d,s);s->blend_failed=1;log_line("Photo Mode alpha blending unavailable; using opaque panel.");return 0;
}
static void prepare_blend(Device *d,Swap *s,Frame *f,uint32_t width,uint32_t height,uint32_t x,uint32_t y){
 static float linear[256];static int initialized;
 if(!initialized){for(int i=0;i<256;++i){float v=i/255.f;linear[i]=v<=.04045f?v/12.92f:powf((v+.055f)/1.055f,2.4f);}initialized=1;}
 int srgb=s->format==VK_FORMAT_B8G8R8A8_SRGB||s->format==VK_FORMAT_R8G8B8A8_SRGB;
 size_t count=(size_t)width*height;unsigned char *bytes=f->pixels;float *rgba=f->pixels;
 /* Backward expansion preserves the unread BGRA pixels in the same upload. */
 for(size_t i=count;i-->0;){unsigned b=bytes[i*4],g=bytes[i*4+1],r=bytes[i*4+2],a=bytes[i*4+3];rgba[i*4]=srgb?linear[r]:r/255.f;rgba[i*4+1]=srgb?linear[g]:g/255.f;rgba[i*4+2]=srgb?linear[b]:b/255.f;rgba[i*4+3]=a/255.f;}
 float left=2.f*x/s->extent.width-1,right=2.f*(x+width)/s->extent.width-1,top=2.f*y/s->extent.height-1,bottom=2.f*(y+height)/s->extent.height-1;
 float vertices[]={left,top,right,top,left,bottom,left,bottom,right,top,right,bottom};memcpy(rgba+count*4,vertices,sizeof(vertices));
 VkDescriptorBufferInfo buffer={f->buffer,0,count*16};
 VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->blend_set,.dstBinding=0,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
 d->UpdateDescriptorSets(d->device,1,&write,0,NULL);
}
static void draw_blend(Device *d,Swap *s,Frame *f,uint32_t index,uint32_t width,uint32_t height,uint32_t x,uint32_t y){
 VkBufferMemoryBarrier host={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=f->buffer,.size=VK_WHOLE_SIZE};
 d->CmdPipelineBarrier(f->command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,NULL,1,&host,0,NULL);
 VkRenderPassBeginInfo begin={.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,.renderPass=s->blend_pass,.framebuffer=s->blend_frames[index],.renderArea={{0,0},s->extent}};
 d->CmdBeginRenderPass(f->command,&begin,VK_SUBPASS_CONTENTS_INLINE);d->CmdBindPipeline(f->command,VK_PIPELINE_BIND_POINT_GRAPHICS,s->blend_pipeline);
 VkViewport viewport={0,0,(float)s->extent.width,(float)s->extent.height,0,1};VkRect2D scissor={{(int32_t)x,(int32_t)y},{width,height}};
 d->CmdSetViewport(f->command,0,1,&viewport);d->CmdSetScissor(f->command,0,1,&scissor);
 d->CmdBindDescriptorSets(f->command,VK_PIPELINE_BIND_POINT_GRAPHICS,s->blend_layout,0,1,&f->blend_set,0,NULL);
 uint32_t constants[]={x,y,width};d->CmdPushConstants(f->command,s->blend_layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(constants),constants);
 VkDeviceSize offset=(VkDeviceSize)width*height*16;d->CmdBindVertexBuffers(f->command,0,1,&f->buffer,&offset);d->CmdDraw(f->command,6,1,0,0);d->CmdEndRenderPass(f->command);
}
#endif

#define WIN32

// https://github.com/LunarG/VulkanSamples/blob/master/API-Samples/15-draw_cube/15-draw_cube.cpp
#include "../main_loop.h"
#include "cube_data.h"
#include <assert.h>
#include <iostream>
#include <string.h>
#include <vuloxr/vk.h>
#include <vuloxr/vk/shaderc.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const char VS[] = {
#embed "15-draw_cube.vert"
    , 0, 0, 0, 0,
};
const char FS[] = {
#embed "15-draw_cube.frag"
    , 0, 0, 0, 0,
};

/* Number of descriptor sets needs to be the same at alloc,       */
/* pipeline layout creation, and descriptor set layout creation   */
#define NUM_DESCRIPTOR_SETS 1

/* Number of samples needs to be the same at image creation,      */
/* renderpass creation and pipeline creation.                     */
#define NUM_SAMPLES VK_SAMPLE_COUNT_1_BIT

/* Number of viewports and number of scissors have to be the same */
/* at pipeline creation and in any call to set them dynamically   */
/* They also have to be the same as each other                    */
#define NUM_VIEWPORTS 1
#define NUM_SCISSORS NUM_VIEWPORTS

/* Amount of time, in nanoseconds, to wait for a command buffer to complete */
#define FENCE_TIMEOUT 100000000

/*
 * Keep each of our swap chain buffers' image, command buffer and view in one
 * spot
 */
typedef struct _swap_chain_buffers {
  VkImage image;
  VkImageView view;
} swap_chain_buffer;

/*
 * Structure for tracking information used / created / modified
 * by utility functions.
 */
struct sample_info {
  VkInstance inst;
  std::vector<VkPhysicalDevice> gpus;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  uint32_t graphics_queue_family_index;
  uint32_t present_queue_family_index;
  VkPhysicalDeviceProperties gpu_props;
  std::vector<VkQueueFamilyProperties> queue_props;
  VkPhysicalDeviceMemoryProperties memory_properties;

  VkFramebuffer *framebuffers;
  int width, height;
  VkFormat format;

  uint32_t swapchainImageCount;
  VkSwapchainKHR swap_chain;
  std::vector<swap_chain_buffer> buffers;
  VkSemaphore imageAcquiredSemaphore;

  struct {
    VkFormat format;

    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
  } depth;

  struct {
    VkBuffer buf;
    VkDeviceMemory mem;
    VkDescriptorBufferInfo buffer_info;
  } uniform_data;

  struct {
    VkDescriptorImageInfo image_info;
  } texture_data;

  struct {
    VkBuffer buf;
    VkDeviceMemory mem;
    VkDescriptorBufferInfo buffer_info;
  } vertex_buffer;
  VkVertexInputBindingDescription vi_binding;
  VkVertexInputAttributeDescription vi_attribs[2];

  glm::mat4 Projection;
  glm::mat4 View;
  glm::mat4 Model;
  glm::mat4 Clip;
  glm::mat4 MVP;

  // VkCommandBuffer cmd; // Buffer for initialization commands
  VkPipelineLayout pipeline_layout;
  std::vector<VkDescriptorSetLayout> desc_layout;
  VkPipelineCache pipelineCache;
  VkRenderPass render_pass;
  VkPipeline pipeline;

  VkPipelineShaderStageCreateInfo shaderStages[2];

  VkDescriptorPool desc_pool;
  std::vector<VkDescriptorSet> desc_set;

  PFN_vkCreateDebugReportCallbackEXT dbgCreateDebugReportCallback;
  PFN_vkDestroyDebugReportCallbackEXT dbgDestroyDebugReportCallback;
  PFN_vkDebugReportMessageEXT dbgBreakCallback;
  std::vector<VkDebugReportCallbackEXT> debug_report_callbacks;

  uint32_t current_buffer;
  uint32_t queue_family_count;

  VkViewport viewport;
  VkRect2D scissor;
};

static bool memory_type_from_properties(struct sample_info &info,
                                        uint32_t typeBits,
                                        VkFlags requirements_mask,
                                        uint32_t *typeIndex) {
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < info.memory_properties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((info.memory_properties.memoryTypes[i].propertyFlags &
           requirements_mask) == requirements_mask) {
        *typeIndex = i;
        return true;
      }
    }
    typeBits >>= 1;
  }
  // No memory types matched, return failure
  return false;
}

static void init_uniform_buffer(struct sample_info &info) {
  VkResult res;
  bool pass;
  float fov = glm::radians(45.0f);
  if (info.width > info.height) {
    fov *= static_cast<float>(info.height) / static_cast<float>(info.width);
  }
  info.Projection = glm::perspective(
      fov, static_cast<float>(info.width) / static_cast<float>(info.height),
      0.1f, 100.0f);
  info.View = glm::lookAt(
      glm::vec3(-5, 3, -10), // Camera is at (-5,3,-10), in World Space
      glm::vec3(0, 0, 0),    // and looks at the origin
      glm::vec3(0, -1, 0)    // Head is up (set to 0,-1,0 to look upside-down)
  );
  info.Model = glm::mat4(1.0f);
  // Vulkan clip space has inverted Y and half Z.
  info.Clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

  info.MVP = info.Clip * info.Projection * info.View * info.Model;

  /* VULKAN_KEY_START */
  VkBufferCreateInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.pNext = NULL;
  buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buf_info.size = sizeof(info.MVP);
  buf_info.queueFamilyIndexCount = 0;
  buf_info.pQueueFamilyIndices = NULL;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buf_info.flags = 0;
  res = vkCreateBuffer(info.device, &buf_info, NULL, &info.uniform_data.buf);
  assert(res == VK_SUCCESS);

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(info.device, info.uniform_data.buf, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = NULL;
  alloc_info.memoryTypeIndex = 0;

  alloc_info.allocationSize = mem_reqs.size;
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &alloc_info.memoryTypeIndex);
  assert(pass && "No mappable, coherent memory");

  res = vkAllocateMemory(info.device, &alloc_info, NULL,
                         &(info.uniform_data.mem));
  assert(res == VK_SUCCESS);

  uint8_t *pData;
  res = vkMapMemory(info.device, info.uniform_data.mem, 0, mem_reqs.size, 0,
                    (void **)&pData);
  assert(res == VK_SUCCESS);

  memcpy(pData, &info.MVP, sizeof(info.MVP));

  vkUnmapMemory(info.device, info.uniform_data.mem);

  res = vkBindBufferMemory(info.device, info.uniform_data.buf,
                           info.uniform_data.mem, 0);
  assert(res == VK_SUCCESS);

  info.uniform_data.buffer_info.buffer = info.uniform_data.buf;
  info.uniform_data.buffer_info.offset = 0;
  info.uniform_data.buffer_info.range = sizeof(info.MVP);
}

static void init_descriptor_and_pipeline_layouts(
    struct sample_info &info, bool use_texture,
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0) {
  VkDescriptorSetLayoutBinding layout_bindings[2];
  layout_bindings[0].binding = 0;
  layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layout_bindings[0].descriptorCount = 1;
  layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  layout_bindings[0].pImmutableSamplers = NULL;

  if (use_texture) {
    layout_bindings[1].binding = 1;
    layout_bindings[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_bindings[1].descriptorCount = 1;
    layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings[1].pImmutableSamplers = NULL;
  }

  /* Next take layout bindings and use them to create a descriptor set layout
   */
  VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
  descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout.pNext = NULL;
  descriptor_layout.flags = descSetLayoutCreateFlags;
  descriptor_layout.bindingCount = use_texture ? 2 : 1;
  descriptor_layout.pBindings = layout_bindings;

  VkResult res;

  info.desc_layout.resize(NUM_DESCRIPTOR_SETS);
  res = vkCreateDescriptorSetLayout(info.device, &descriptor_layout, NULL,
                                    info.desc_layout.data());
  assert(res == VK_SUCCESS);

  /* Now use the descriptor layout to create a pipeline layout */
  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
  pPipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pPipelineLayoutCreateInfo.pNext = NULL;
  pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
  pPipelineLayoutCreateInfo.pPushConstantRanges = NULL;
  pPipelineLayoutCreateInfo.setLayoutCount = NUM_DESCRIPTOR_SETS;
  pPipelineLayoutCreateInfo.pSetLayouts = info.desc_layout.data();

  res = vkCreatePipelineLayout(info.device, &pPipelineLayoutCreateInfo, NULL,
                               &info.pipeline_layout);
  assert(res == VK_SUCCESS);
}

static void
init_renderpass(struct sample_info &info, bool include_depth, bool clear = true,
                VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED) {
  /* DEPENDS on init_swap_chain() and init_depth_buffer() */

  assert(clear || (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED));

  VkResult res;
  /* Need attachments for render target and depth buffer */
  VkAttachmentDescription attachments[2];
  attachments[0].format = info.format;
  attachments[0].samples = NUM_SAMPLES;
  attachments[0].loadOp =
      clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = initialLayout;
  attachments[0].finalLayout = finalLayout;
  attachments[0].flags = 0;

  if (include_depth) {
    attachments[1].format = info.depth.format;
    attachments[1].samples = NUM_SAMPLES;
    attachments[1].loadOp =
        clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags = 0;
  }

  VkAttachmentReference color_reference = {};
  color_reference.attachment = 0;
  color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_reference = {};
  depth_reference.attachment = 1;
  depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.flags = 0;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_reference;
  subpass.pResolveAttachments = NULL;
  subpass.pDepthStencilAttachment = include_depth ? &depth_reference : NULL;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  // Subpass dependency to wait for wsi image acquired semaphore before starting
  // layout transition
  VkSubpassDependency subpass_dependency = {};
  subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependency.dstSubpass = 0;
  subpass_dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dependency.srcAccessMask = 0;
  subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_dependency.dependencyFlags = 0;

  VkRenderPassCreateInfo rp_info = {};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.pNext = NULL;
  rp_info.attachmentCount = include_depth ? 2 : 1;
  rp_info.pAttachments = attachments;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 1;
  rp_info.pDependencies = &subpass_dependency;

  res = vkCreateRenderPass(info.device, &rp_info, NULL, &info.render_pass);
  assert(res == VK_SUCCESS);
}

static void init_vertex_buffer(struct sample_info &info, const void *vertexData,
                               uint32_t dataSize, uint32_t dataStride,
                               bool use_texture) {
  VkResult res;
  bool pass;

  VkBufferCreateInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.pNext = NULL;
  buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  buf_info.size = dataSize;
  buf_info.queueFamilyIndexCount = 0;
  buf_info.pQueueFamilyIndices = NULL;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buf_info.flags = 0;
  res = vkCreateBuffer(info.device, &buf_info, NULL, &info.vertex_buffer.buf);
  assert(res == VK_SUCCESS);

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(info.device, info.vertex_buffer.buf, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = NULL;
  alloc_info.memoryTypeIndex = 0;

  alloc_info.allocationSize = mem_reqs.size;
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &alloc_info.memoryTypeIndex);
  assert(pass && "No mappable, coherent memory");

  res = vkAllocateMemory(info.device, &alloc_info, NULL,
                         &(info.vertex_buffer.mem));
  assert(res == VK_SUCCESS);
  info.vertex_buffer.buffer_info.range = mem_reqs.size;
  info.vertex_buffer.buffer_info.offset = 0;

  uint8_t *pData;
  res = vkMapMemory(info.device, info.vertex_buffer.mem, 0, mem_reqs.size, 0,
                    (void **)&pData);
  assert(res == VK_SUCCESS);

  memcpy(pData, vertexData, dataSize);

  vkUnmapMemory(info.device, info.vertex_buffer.mem);

  res = vkBindBufferMemory(info.device, info.vertex_buffer.buf,
                           info.vertex_buffer.mem, 0);
  assert(res == VK_SUCCESS);

  info.vi_binding.binding = 0;
  info.vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  info.vi_binding.stride = dataStride;

  info.vi_attribs[0].binding = 0;
  info.vi_attribs[0].location = 0;
  info.vi_attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  info.vi_attribs[0].offset = 0;
  info.vi_attribs[1].binding = 0;
  info.vi_attribs[1].location = 1;
  info.vi_attribs[1].format =
      use_texture ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
  info.vi_attribs[1].offset = 16;
}

static void init_framebuffers(struct sample_info &info, bool include_depth) {
  /* DEPENDS on init_depth_buffer(), init_renderpass() and
   * init_swapchain_extension() */

  VkResult res;
  VkImageView attachments[2];
  attachments[1] = info.depth.view;

  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = NULL;
  fb_info.renderPass = info.render_pass;
  fb_info.attachmentCount = include_depth ? 2 : 1;
  fb_info.pAttachments = attachments;
  fb_info.width = info.width;
  fb_info.height = info.height;
  fb_info.layers = 1;

  uint32_t i;

  info.framebuffers =
      (VkFramebuffer *)malloc(info.swapchainImageCount * sizeof(VkFramebuffer));

  for (i = 0; i < info.swapchainImageCount; i++) {
    attachments[0] = info.buffers[i].view;
    res =
        vkCreateFramebuffer(info.device, &fb_info, NULL, &info.framebuffers[i]);
    assert(res == VK_SUCCESS);
  }
}

static void init_descriptor_pool(struct sample_info &info, bool use_texture) {
  /* DEPENDS on init_uniform_buffer() and
   * init_descriptor_and_pipeline_layouts() */

  VkResult res;
  VkDescriptorPoolSize type_count[2];
  type_count[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  type_count[0].descriptorCount = 1;
  if (use_texture) {
    type_count[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_count[1].descriptorCount = 1;
  }

  VkDescriptorPoolCreateInfo descriptor_pool = {};
  descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool.pNext = NULL;
  descriptor_pool.maxSets = 1;
  descriptor_pool.poolSizeCount = use_texture ? 2 : 1;
  descriptor_pool.pPoolSizes = type_count;

  res = vkCreateDescriptorPool(info.device, &descriptor_pool, NULL,
                               &info.desc_pool);
  assert(res == VK_SUCCESS);
}

static void init_descriptor_set(struct sample_info &info, bool use_texture) {
  /* DEPENDS on init_descriptor_pool() */

  VkResult res;

  VkDescriptorSetAllocateInfo alloc_info[1];
  alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info[0].pNext = NULL;
  alloc_info[0].descriptorPool = info.desc_pool;
  alloc_info[0].descriptorSetCount = NUM_DESCRIPTOR_SETS;
  alloc_info[0].pSetLayouts = info.desc_layout.data();

  info.desc_set.resize(NUM_DESCRIPTOR_SETS);
  res = vkAllocateDescriptorSets(info.device, alloc_info, info.desc_set.data());
  assert(res == VK_SUCCESS);

  VkWriteDescriptorSet writes[2];

  writes[0] = {};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = NULL;
  writes[0].dstSet = info.desc_set[0];
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &info.uniform_data.buffer_info;
  writes[0].dstArrayElement = 0;
  writes[0].dstBinding = 0;

  if (use_texture) {
    writes[1] = {};
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = info.desc_set[0];
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &info.texture_data.image_info;
    writes[1].dstArrayElement = 0;
  }

  vkUpdateDescriptorSets(info.device, use_texture ? 2 : 1, writes, 0, NULL);
}

static void init_shaders(struct sample_info &info,
                         const VkShaderModuleCreateInfo *vertShaderCI,
                         const VkShaderModuleCreateInfo *fragShaderCI) {
  VkResult res;

  if (vertShaderCI) {
    info.shaderStages[0].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.shaderStages[0].pNext = NULL;
    info.shaderStages[0].pSpecializationInfo = NULL;
    info.shaderStages[0].flags = 0;
    info.shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    info.shaderStages[0].pName = "main";
    res = vkCreateShaderModule(info.device, vertShaderCI, NULL,
                               &info.shaderStages[0].module);
    assert(res == VK_SUCCESS);
  }

  if (fragShaderCI) {
    std::vector<unsigned int> vtx_spv;
    info.shaderStages[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.shaderStages[1].pNext = NULL;
    info.shaderStages[1].pSpecializationInfo = NULL;
    info.shaderStages[1].flags = 0;
    info.shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    info.shaderStages[1].pName = "main";
    res = vkCreateShaderModule(info.device, fragShaderCI, NULL,
                               &info.shaderStages[1].module);
    assert(res == VK_SUCCESS);
  }
}

static void init_pipeline_cache(struct sample_info &info) {
  VkResult res;

  VkPipelineCacheCreateInfo pipelineCache;
  pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  pipelineCache.pNext = NULL;
  pipelineCache.initialDataSize = 0;
  pipelineCache.pInitialData = NULL;
  pipelineCache.flags = 0;
  res = vkCreatePipelineCache(info.device, &pipelineCache, NULL,
                              &info.pipelineCache);
  assert(res == VK_SUCCESS);
}

static void init_pipeline(struct sample_info &info, VkBool32 include_depth,
                          VkBool32 include_vi = true) {
  VkResult res;

  VkDynamicState dynamicStateEnables[2]; // Viewport + Scissor
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pNext = NULL;
  dynamicState.pDynamicStates = dynamicStateEnables;
  dynamicState.dynamicStateCount = 0;

  VkPipelineVertexInputStateCreateInfo vi;
  memset(&vi, 0, sizeof(vi));
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  if (include_vi) {
    vi.pNext = NULL;
    vi.flags = 0;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &info.vi_binding;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = info.vi_attribs;
  }
  VkPipelineInputAssemblyStateCreateInfo ia;
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.pNext = NULL;
  ia.flags = 0;
  ia.primitiveRestartEnable = VK_FALSE;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineRasterizationStateCreateInfo rs;
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.pNext = NULL;
  rs.flags = 0;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.depthBiasEnable = VK_FALSE;
  rs.depthBiasConstantFactor = 0;
  rs.depthBiasClamp = 0;
  rs.depthBiasSlopeFactor = 0;
  rs.lineWidth = 1.0f;

  VkPipelineColorBlendStateCreateInfo cb;
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.flags = 0;
  cb.pNext = NULL;
  VkPipelineColorBlendAttachmentState att_state[1];
  att_state[0].colorWriteMask = 0xf;
  att_state[0].blendEnable = VK_FALSE;
  att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
  att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
  att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  cb.attachmentCount = 1;
  cb.pAttachments = att_state;
  cb.logicOpEnable = VK_FALSE;
  cb.logicOp = VK_LOGIC_OP_NO_OP;
  cb.blendConstants[0] = 1.0f;
  cb.blendConstants[1] = 1.0f;
  cb.blendConstants[2] = 1.0f;
  cb.blendConstants[3] = 1.0f;

  VkPipelineViewportStateCreateInfo vp = {};
  vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.pNext = NULL;
  vp.flags = 0;
#ifndef __ANDROID__
  vp.viewportCount = NUM_VIEWPORTS;
  dynamicStateEnables[dynamicState.dynamicStateCount++] =
      VK_DYNAMIC_STATE_VIEWPORT;
  vp.scissorCount = NUM_SCISSORS;
  dynamicStateEnables[dynamicState.dynamicStateCount++] =
      VK_DYNAMIC_STATE_SCISSOR;
  vp.pScissors = NULL;
  vp.pViewports = NULL;
#else
  // Temporary disabling dynamic viewport on Android because some of drivers
  // doesn't support the feature.
  VkViewport viewports;
  viewports.minDepth = 0.0f;
  viewports.maxDepth = 1.0f;
  viewports.x = 0;
  viewports.y = 0;
  viewports.width = info.width;
  viewports.height = info.height;
  VkRect2D scissor;
  scissor.extent.width = info.width;
  scissor.extent.height = info.height;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  vp.viewportCount = NUM_VIEWPORTS;
  vp.scissorCount = NUM_SCISSORS;
  vp.pScissors = &scissor;
  vp.pViewports = &viewports;
#endif
  VkPipelineDepthStencilStateCreateInfo ds;
  ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ds.pNext = NULL;
  ds.flags = 0;
  ds.depthTestEnable = include_depth;
  ds.depthWriteEnable = include_depth;
  ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds.depthBoundsTestEnable = VK_FALSE;
  ds.stencilTestEnable = VK_FALSE;
  ds.back.failOp = VK_STENCIL_OP_KEEP;
  ds.back.passOp = VK_STENCIL_OP_KEEP;
  ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.back.compareMask = 0;
  ds.back.reference = 0;
  ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
  ds.back.writeMask = 0;
  ds.minDepthBounds = 0;
  ds.maxDepthBounds = 0;
  ds.stencilTestEnable = VK_FALSE;
  ds.front = ds.back;

  VkPipelineMultisampleStateCreateInfo ms;
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.pNext = NULL;
  ms.flags = 0;
  ms.pSampleMask = NULL;
  ms.rasterizationSamples = NUM_SAMPLES;
  ms.sampleShadingEnable = VK_FALSE;
  ms.alphaToCoverageEnable = VK_FALSE;
  ms.alphaToOneEnable = VK_FALSE;
  ms.minSampleShading = 0.0;

  VkGraphicsPipelineCreateInfo pipeline;
  pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline.pNext = NULL;
  pipeline.layout = info.pipeline_layout;
  pipeline.basePipelineHandle = VK_NULL_HANDLE;
  pipeline.basePipelineIndex = 0;
  pipeline.flags = 0;
  pipeline.pVertexInputState = &vi;
  pipeline.pInputAssemblyState = &ia;
  pipeline.pRasterizationState = &rs;
  pipeline.pColorBlendState = &cb;
  pipeline.pTessellationState = NULL;
  pipeline.pMultisampleState = &ms;
  pipeline.pDynamicState = &dynamicState;
  pipeline.pViewportState = &vp;
  pipeline.pDepthStencilState = &ds;
  pipeline.pStages = info.shaderStages;
  pipeline.stageCount = 2;
  pipeline.renderPass = info.render_pass;
  pipeline.subpass = 0;

  res = vkCreateGraphicsPipelines(info.device, info.pipelineCache, 1, &pipeline,
                                  NULL, &info.pipeline);
  assert(res == VK_SUCCESS);
}

static VkFormat depthFormat() {
  auto format = VK_FORMAT_UNDEFINED;
  /* allow custom depth formats */
#ifdef __ANDROID__
  // Depth format needs to be VK_FORMAT_D24_UNORM_S8_UINT on
  // Android (if available).
  vkGetPhysicalDeviceFormatProperties(info.gpus[0], VK_FORMAT_D24_UNORM_S8_UINT,
                                      &props);
  if ((props.linearTilingFeatures &
       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
      (props.optimalTilingFeatures &
       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    info.depth.format = VK_FORMAT_D24_UNORM_S8_UINT;
  else
    info.depth.format = VK_FORMAT_D16_UNORM;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
  if (info.depth.format == VK_FORMAT_UNDEFINED)
    info.depth.format = VK_FORMAT_D32_SFLOAT;
#else
  if (format == VK_FORMAT_UNDEFINED)
    format = VK_FORMAT_D16_UNORM;
#endif
  return format;
}

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *window) {
  struct sample_info info = {
      .inst = instance,
      .gpus = {physicalDevice},
      .device = device,
      .memory_properties = physicalDevice.memoryProps,
      .width = static_cast<int>(swapchain.createInfo.imageExtent.width),
      .height = static_cast<int>(swapchain.createInfo.imageExtent.height),
      .format = swapchain.createInfo.imageFormat,
      .swapchainImageCount = static_cast<uint32_t>(swapchain.images.size()),
      .swap_chain = swapchain,
  };
  vkGetDeviceQueue(info.device, info.graphics_queue_family_index, 0,
                   &info.graphics_queue);
  if (info.graphics_queue_family_index == info.present_queue_family_index) {
    info.present_queue = info.graphics_queue;
  } else {
    vkGetDeviceQueue(info.device, info.present_queue_family_index, 0,
                     &info.present_queue);
  }

  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    VkImageViewCreateInfo color_image_view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = swapchain.images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = info.format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkImageView view;
    auto res = vkCreateImageView(info.device, &color_image_view, NULL, &view);

    info.buffers.push_back({
        .image = color_image_view.image,
        .view = view,
    });
    assert(res == VK_SUCCESS);
  }
  info.current_buffer = 0;

  const bool depthPresent = true;

  const VkFormat depth_format = info.depth.format = depthFormat();

  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(info.gpus[0], depth_format, &props);

  VkImageTiling tiling;
  if (props.linearTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    tiling = VK_IMAGE_TILING_LINEAR;
  } else if (props.optimalTilingFeatures &
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    tiling = VK_IMAGE_TILING_OPTIMAL;
  } else {
    /* Try other depth formats? */
    std::cout << "depth_format " << depth_format << " Unsupported.\n";
    exit(-1);
  }

  VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depth_format,
      .extent =
          {
              .width = static_cast<uint32_t>(info.width),
              .height = static_cast<uint32_t>(info.height),
              .depth = 1,
          },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = NUM_SAMPLES,
      .tiling = tiling,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkMemoryAllocateInfo mem_alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = NULL,
      .allocationSize = 0,
      .memoryTypeIndex = 0,
  };

  VkImageViewCreateInfo view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .image = VK_NULL_HANDLE,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_format,
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_R,
              .g = VK_COMPONENT_SWIZZLE_G,
              .b = VK_COMPONENT_SWIZZLE_B,
              .a = VK_COMPONENT_SWIZZLE_A,
          },
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  if (depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
      depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
      depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
    view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  /* Create image */
  auto res = vkCreateImage(info.device, &image_info, NULL, &info.depth.image);
  assert(res == VK_SUCCESS);

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(info.device, info.depth.image, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;
  /* Use the memory properties to determine the type of memory required */
  auto pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                          &mem_alloc.memoryTypeIndex);
  assert(pass);

  /* Allocate memory */
  res = vkAllocateMemory(info.device, &mem_alloc, NULL, &info.depth.mem);
  assert(res == VK_SUCCESS);

  /* Bind memory */
  res = vkBindImageMemory(info.device, info.depth.image, info.depth.mem, 0);
  assert(res == VK_SUCCESS);

  /* Create image view */
  view_info.image = info.depth.image;
  res = vkCreateImageView(info.device, &view_info, NULL, &info.depth.view);
  assert(res == VK_SUCCESS);
  // }

  init_uniform_buffer(info);
  init_descriptor_and_pipeline_layouts(info, false);
  init_renderpass(info, depthPresent);

  auto vs = vuloxr::vk::glsl_vs_to_spv(VS);
  VkShaderModuleCreateInfo vert_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = static_cast<uint32_t>(vs.size() * 4),
      .pCode = vs.data(),
  };

  auto fs = vuloxr::vk::glsl_fs_to_spv(FS);
  VkShaderModuleCreateInfo frag_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = static_cast<uint32_t>(fs.size() * 4),
      .pCode = fs.data(),
  };
  init_shaders(info, &vert_info, &frag_info);
  init_framebuffers(info, depthPresent);
  init_vertex_buffer(info, g_vb_solid_face_colors_Data,
                     sizeof(g_vb_solid_face_colors_Data),
                     sizeof(g_vb_solid_face_colors_Data[0]), false);
  init_descriptor_pool(info, false);
  init_descriptor_set(info, false);
  init_pipeline_cache(info);
  init_pipeline(info, depthPresent);

  /* VULKAN_KEY_START */

  VkClearValue clear_values[2];
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.2f;
  clear_values[0].color.float32[3] = 0.2f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;

  VkSemaphore imageAcquiredSemaphore;
  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
  imageAcquiredSemaphoreCreateInfo.sType =
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  imageAcquiredSemaphoreCreateInfo.pNext = NULL;
  imageAcquiredSemaphoreCreateInfo.flags = 0;

  res = vkCreateSemaphore(info.device, &imageAcquiredSemaphoreCreateInfo, NULL,
                          &imageAcquiredSemaphore);
  assert(res == VK_SUCCESS);

  VkCommandPool cmd_pool;
  VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = info.graphics_queue_family_index,
  };
  res = vkCreateCommandPool(info.device, &cmd_pool_info, NULL, &cmd_pool);
  assert(res == VK_SUCCESS);

  while (runLoop()) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    res = vkAllocateCommandBuffers(info.device, &cmdInfo, &cmd);
    assert(res == VK_SUCCESS);

    // Get the index of the next available swapchain image:
    res = vkAcquireNextImageKHR(info.device, info.swap_chain, UINT64_MAX,
                                imageAcquiredSemaphore, VK_NULL_HANDLE,
                                &info.current_buffer);
    // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
    // return codes
    assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };
    res = vkBeginCommandBuffer(cmd, &cmd_buf_info);
    assert(res == VK_SUCCESS);

    VkRenderPassBeginInfo rp_begin;
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = NULL;
    rp_begin.renderPass = info.render_pass;
    rp_begin.framebuffer = info.framebuffers[info.current_buffer];
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;
    rp_begin.renderArea.extent.width = info.width;
    rp_begin.renderArea.extent.height = info.height;
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, info.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            info.pipeline_layout, 0, NUM_DESCRIPTOR_SETS,
                            info.desc_set.data(), 0, NULL);

    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &info.vertex_buffer.buf, offsets);

    info.viewport.height = (float)info.height;
    info.viewport.width = (float)info.width;
    info.viewport.minDepth = (float)0.0f;
    info.viewport.maxDepth = (float)1.0f;
    info.viewport.x = 0;
    info.viewport.y = 0;
    vkCmdSetViewport(cmd, 0, NUM_VIEWPORTS, &info.viewport);

    info.scissor.extent.width = info.width;
    info.scissor.extent.height = info.height;
    info.scissor.offset.x = 0;
    info.scissor.offset.y = 0;
    vkCmdSetScissor(cmd, 0, NUM_SCISSORS, &info.scissor);

    vkCmdDraw(cmd, 12 * 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    res = vkEndCommandBuffer(cmd);
    const VkCommandBuffer cmd_bufs[] = {cmd};
    VkFenceCreateInfo fenceInfo;
    VkFence drawFence;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = NULL;
    fenceInfo.flags = 0;
    vkCreateFence(info.device, &fenceInfo, NULL, &drawFence);

    VkPipelineStageFlags pipe_stage_flags =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info[1] = {};
    submit_info[0].pNext = NULL;
    submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 1;
    submit_info[0].pWaitSemaphores = &imageAcquiredSemaphore;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = cmd_bufs;
    submit_info[0].signalSemaphoreCount = 0;
    submit_info[0].pSignalSemaphores = NULL;

    /* Queue the command buffer for execution */
    res = vkQueueSubmit(info.graphics_queue, 1, submit_info, drawFence);
    assert(res == VK_SUCCESS);

    /* Now present the image in the window */

    VkPresentInfoKHR present;
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = NULL;
    present.swapchainCount = 1;
    present.pSwapchains = &info.swap_chain;
    present.pImageIndices = &info.current_buffer;
    present.pWaitSemaphores = NULL;
    present.waitSemaphoreCount = 0;
    present.pResults = NULL;

    /* Make sure command buffer is finished before presenting */
    do {
      res = vkWaitForFences(info.device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    } while (res == VK_TIMEOUT);

    assert(res == VK_SUCCESS);
    res = vkQueuePresentKHR(info.present_queue, &present);
    assert(res == VK_SUCCESS);

    vkDestroyFence(info.device, drawFence, NULL);

    vkFreeCommandBuffers(info.device, cmd_pool, 1, cmd_bufs);
  }

  vkDestroySemaphore(info.device, imageAcquiredSemaphore, NULL);

  vkDestroyPipeline(info.device, info.pipeline, NULL);

  vkDestroyPipelineCache(info.device, info.pipelineCache, NULL);

  vkDestroyDescriptorPool(info.device, info.desc_pool, NULL);

  vkDestroyBuffer(info.device, info.vertex_buffer.buf, NULL);
  vkFreeMemory(info.device, info.vertex_buffer.mem, NULL);

  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    vkDestroyFramebuffer(info.device, info.framebuffers[i], NULL);
  }
  free(info.framebuffers);

  vkDestroyShaderModule(info.device, info.shaderStages[0].module, NULL);
  vkDestroyShaderModule(info.device, info.shaderStages[1].module, NULL);

  vkDestroyRenderPass(info.device, info.render_pass, NULL);

  for (int i = 0; i < NUM_DESCRIPTOR_SETS; i++)
    vkDestroyDescriptorSetLayout(info.device, info.desc_layout[i], NULL);
  vkDestroyPipelineLayout(info.device, info.pipeline_layout, NULL);

  vkDestroyBuffer(info.device, info.uniform_data.buf, NULL);
  vkFreeMemory(info.device, info.uniform_data.mem, NULL);

  vkDestroyImageView(info.device, info.depth.view, NULL);
  vkDestroyImage(info.device, info.depth.image, NULL);
  vkFreeMemory(info.device, info.depth.mem, NULL);

  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    vkDestroyImageView(info.device, info.buffers[i].view, NULL);
  }

  vkDestroyCommandPool(info.device, cmd_pool, NULL);
}

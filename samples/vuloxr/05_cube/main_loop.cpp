#define WIN32

// https://github.com/LunarG/VulkanSamples/blob/master/API-Samples/15-draw_cube/15-draw_cube.cpp
#include "../main_loop.h"
#include "cube_data.h"
#include <assert.h>
#include <string.h>
#include <vuloxr/vk.h>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
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

static bool memory_type_from_properties(
    const VkPhysicalDeviceMemoryProperties &memory_properties,
    uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((memory_properties.memoryTypes[i].propertyFlags &
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

/*
 * Structure for tracking information used / created / modified
 * by utility functions.
 */

struct UniformData {
  VkBuffer buf;
  VkDeviceMemory mem;
  VkDescriptorBufferInfo buffer_info;
};

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

  UniformData uniform_data;

  glm::mat4 Projection;
  glm::mat4 View;
  glm::mat4 Model;
  glm::mat4 Clip;
  glm::mat4 MVP;

  PFN_vkCreateDebugReportCallbackEXT dbgCreateDebugReportCallback;
  PFN_vkDestroyDebugReportCallbackEXT dbgDestroyDebugReportCallback;
  PFN_vkDebugReportMessageEXT dbgBreakCallback;
  std::vector<VkDebugReportCallbackEXT> debug_report_callbacks;

  uint32_t current_buffer;
  uint32_t queue_family_count;

  VkViewport viewport;
  VkRect2D scissor;

  // sample_info(const sample_info &) = delete;
  sample_info &operator=(const sample_info &) = delete;
};

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
  pass = memory_type_from_properties(info.memory_properties,
                                     mem_reqs.memoryTypeBits,
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

static void init_framebuffers(struct sample_info &info, VkImageView depth_view,
                              VkRenderPass render_pass, bool include_depth) {
  VkResult res;
  VkImageView attachments[2];
  attachments[1] = depth_view;

  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = NULL;
  fb_info.renderPass = render_pass;
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

  sample_info info = {
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

  vuloxr::vk::DepthImage depth(info.device,
                               VkExtent2D{
                                   .width = static_cast<uint32_t>(info.width),
                                   .height = static_cast<uint32_t>(info.height),
                               },
                               depthFormat(), VK_SAMPLE_COUNT_1_BIT);
  VkImageViewCreateInfo view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .image = depth.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth.imageInfo.format,
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
  if (depth.imageInfo.format == VK_FORMAT_D16_UNORM_S8_UINT ||
      depth.imageInfo.format == VK_FORMAT_D24_UNORM_S8_UINT ||
      depth.imageInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
    view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  depth.memory = physicalDevice.allocForTransfer(device, depth.image);
  /* Create image view */
  VkImageView depth_view;
  vuloxr::vk::CheckVkResult(
      vkCreateImageView(info.device, &view_info, NULL, &depth_view));

  init_uniform_buffer(info);

  vuloxr::vk::DescriptorSet descriptor(
      info.device, 1,
      {
          {
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
              .pImmutableSamplers = NULL,
          },
      });

  /* Now use the descriptor layout to create a pipeline layout */
  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor.descriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = NULL,
  };
  VkPipelineLayout pipeline_layout;
  vuloxr::vk::CheckVkResult(vkCreatePipelineLayout(
      info.device, &pPipelineLayoutCreateInfo, NULL, &pipeline_layout));

  auto [render_pass, depthstencil] = vuloxr::vk::createColorDepthRenderPass(
      info.device, info.format, depth.imageInfo.format);

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, vuloxr::vk::glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, vuloxr::vk::glsl_fs_to_spv(FS), "main");

  init_framebuffers(info, depth_view, render_pass, true);

  vuloxr::vk::VertexBuffer vertex_buffer{
      .bindings =
          {
              {
                  .binding = 0,
                  .stride = sizeof(Vertex),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributes = {
          // describes position
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = 0,
          },
          // describes color
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = 16,
          },
      }};
  vertex_buffer.allocate(physicalDevice, device,
                         std::span<const Vertex>(g_vb_solid_face_colors_Data));

  descriptor.update(0, std::span<const vuloxr::vk::DescriptorUpdateInfo>({
                           vuloxr::vk::DescriptorUpdateInfo{
                               .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               .pBufferInfo = &info.uniform_data.buffer_info,
                           },
                       }));

  VkPipelineCacheCreateInfo pipelineCacheInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .initialDataSize = 0,
      .pInitialData = NULL,
  };
  VkPipelineCache pipelineCache;
  vuloxr::vk::CheckVkResult(vkCreatePipelineCache(
      info.device, &pipelineCacheInfo, NULL, &pipelineCache));

  vuloxr::vk::PipelineBuilder builder;
  auto pipeline = builder.create(
      device, render_pass, depthstencil, pipeline_layout,
      {vs.pipelineShaderStageCreateInfo, fs.pipelineShaderStageCreateInfo},
      vertex_buffer.bindings, vertex_buffer.attributes, {}, {},
      {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

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

  vuloxr::vk::CheckVkResult(vkCreateSemaphore(info.device,
                                              &imageAcquiredSemaphoreCreateInfo,
                                              NULL, &imageAcquiredSemaphore));

  VkCommandPool cmd_pool;
  VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = info.graphics_queue_family_index,
  };
  vuloxr::vk::CheckVkResult(
      vkCreateCommandPool(info.device, &cmd_pool_info, NULL, &cmd_pool));

  while (runLoop()) {
    // Get the index of the next available swapchain image:
    vuloxr::vk::CheckVkResult(vkAcquireNextImageKHR(
        info.device, info.swap_chain, UINT64_MAX, imageAcquiredSemaphore,
        VK_NULL_HANDLE, &info.current_buffer));

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vuloxr::vk::CheckVkResult(
        vkAllocateCommandBuffers(info.device, &cmdInfo, &cmd));

    {
      vuloxr::vk::RenderPassRecording recording(
          cmd, pipeline_layout, render_pass,
          info.framebuffers[info.current_buffer],
          swapchain.createInfo.imageExtent, clear_values,
          descriptor.descriptorSets[0],
          VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

      vertex_buffer.draw(cmd, pipeline);
    }

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VkFence drawFence;
    vkCreateFence(info.device, &fenceInfo, NULL, &drawFence);

    device.submit(cmd, imageAcquiredSemaphore, {}, drawFence);

    /* Make sure command buffer is finished before presenting */
    VkResult res;
    do {
      res = vkWaitForFences(info.device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    } while (res == VK_TIMEOUT);
    assert(res == VK_SUCCESS);

    vuloxr::vk::CheckVkResult(swapchain.present(info.current_buffer));

    vkDestroyFence(info.device, drawFence, NULL);
    vkFreeCommandBuffers(info.device, cmd_pool, 1, &cmd);
  }

  vkDestroySemaphore(info.device, imageAcquiredSemaphore, NULL);

  vkDestroyPipelineCache(info.device, pipelineCache, NULL);

  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    vkDestroyFramebuffer(info.device, info.framebuffers[i], NULL);
  }
  free(info.framebuffers);

  vkDestroyBuffer(info.device, info.uniform_data.buf, NULL);
  vkFreeMemory(info.device, info.uniform_data.mem, NULL);

  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    vkDestroyImageView(info.device, info.buffers[i].view, NULL);
  }
  vkDestroyImageView(device, depth_view, NULL);

  vkDestroyCommandPool(info.device, cmd_pool, NULL);
}

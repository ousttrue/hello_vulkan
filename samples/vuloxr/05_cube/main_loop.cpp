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

/* Amount of time, in nanoseconds, to wait for a command buffer to complete */
#define FENCE_TIMEOUT 100000000

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

  int width, height;
  VkFormat format;

  uint32_t swapchainImageCount;
  VkSwapchainKHR swap_chain;
  // std::vector<swap_chain_buffer> buffers;
  VkSemaphore imageAcquiredSemaphore;

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

static glm::mat4 mvp(int width, int height) {
  float fov = glm::radians(45.0f);
  if (width > height) {
    fov *= static_cast<float>(height) / static_cast<float>(width);
  }
  auto Projection = glm::perspective(
      fov, static_cast<float>(width) / static_cast<float>(height), 0.1f,
      100.0f);
  auto View = glm::lookAt(
      glm::vec3(-5, 3, -10), // Camera is at (-5,3,-10), in World Space
      glm::vec3(0, 0, 0),    // and looks at the origin
      glm::vec3(0, -1, 0)    // Head is up (set to 0,-1,0 to look upside-down)
  );
  auto Model = glm::mat4(1.0f);
  // Vulkan clip space has inverted Y and half Z.
  auto Clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

  return Clip * Projection * View * Model;
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

  vuloxr::vk::UniformBuffer<glm::mat4> ubo(info.device);
  ubo.memory = physicalDevice.allocForMap(device, ubo.buffer);
  ubo.value = mvp(info.width, info.height);
  ubo.mapWrite();

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

  auto depth_format = depthFormat();
  auto [render_pass, depthstencil] = vuloxr::vk::createColorDepthRenderPass(
      info.device, info.format, depth_format);

  vuloxr::vk::SwapchainSharedDepthFramebufferList framebuffers(
      physicalDevice, info.device, render_pass,
      swapchain.createInfo.imageExtent, swapchain.createInfo.imageFormat,
      depth_format, VK_SAMPLE_COUNT_1_BIT, swapchain.images);

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, vuloxr::vk::glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, vuloxr::vk::glsl_fs_to_spv(FS), "main");

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
                               .pBufferInfo = &ubo.info,
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

  VkClearValue clear_values[2] = {
      {.color = {0.2f, 0.2f, 0.2f, 0.2f}},
      {.depthStencil = {.depth = 1.0f, .stencil = 0}},
  };

  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
  };
  VkSemaphore imageAcquiredSemaphore;
  vuloxr::vk::CheckVkResult(vkCreateSemaphore(info.device,
                                              &imageAcquiredSemaphoreCreateInfo,
                                              NULL, &imageAcquiredSemaphore));

  VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = info.graphics_queue_family_index,
  };
  VkCommandPool cmd_pool;
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
          framebuffers[info.current_buffer].framebuffer,
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
  vkDestroyCommandPool(info.device, cmd_pool, NULL);
}

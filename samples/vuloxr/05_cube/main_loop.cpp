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

VkClearValue clear_values[2] = {
    {.color = {0.2f, 0.2f, 0.2f, 0.2f}},
    {.depthStencil = {.depth = 1.0f, .stencil = 0}},
};

const char VS[] = {
#embed "15-draw_cube.vert"
    , 0, 0, 0, 0,
};
const char FS[] = {
#embed "15-draw_cube.frag"
    , 0, 0, 0, 0,
};

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *window) {

  vuloxr::vk::UniformBuffer<glm::mat4> ubo(device);
  ubo.memory = physicalDevice.allocForMap(device, ubo.buffer);
  ubo.value = mvp(swapchain.createInfo.imageExtent.width,
                  swapchain.createInfo.imageExtent.height);
  ubo.mapWrite();

  vuloxr::vk::DescriptorSet descriptor(
      device, 1,
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
      device, &pPipelineLayoutCreateInfo, NULL, &pipeline_layout));

  auto depth_format = physicalDevice.depthFormat();
  auto [render_pass, depthstencil] = vuloxr::vk::createColorDepthRenderPass(
      device, swapchain.createInfo.imageFormat, depth_format);

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
  vuloxr::vk::CheckVkResult(
      vkCreatePipelineCache(device, &pipelineCacheInfo, NULL, &pipelineCache));

  vuloxr::vk::PipelineBuilder builder;
  auto pipeline = builder.create(
      device, render_pass, depthstencil, pipeline_layout,
      {vs.pipelineShaderStageCreateInfo, fs.pipelineShaderStageCreateInfo},
      vertex_buffer.bindings, vertex_buffer.attributes, {}, {},
      {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  vuloxr::vk::SwapchainSharedDepthFramebufferList framebuffers(
      device, swapchain.createInfo.imageFormat, depth_format,
      VK_SAMPLE_COUNT_1_BIT);
  framebuffers.reset(physicalDevice, render_pass,
                     swapchain.createInfo.imageExtent, swapchain.images);

  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
  };
  VkSemaphore imageAcquiredSemaphore;
  vuloxr::vk::CheckVkResult(vkCreateSemaphore(device,
                                              &imageAcquiredSemaphoreCreateInfo,
                                              NULL, &imageAcquiredSemaphore));

  VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = physicalDevice.graphicsFamilyIndex,
  };
  VkCommandPool cmd_pool;
  vuloxr::vk::CheckVkResult(
      vkCreateCommandPool(device, &cmd_pool_info, NULL, &cmd_pool));

  while (auto state = windowLoopOnce()) {
    uint32_t imageIndex;
    vuloxr::vk::CheckVkResult(vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE,
        &imageIndex));

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vuloxr::vk::CheckVkResult(vkAllocateCommandBuffers(device, &cmdInfo, &cmd));

    {
      vuloxr::vk::RenderPassRecording recording(
          cmd, pipeline_layout, render_pass,
          framebuffers[imageIndex].framebuffer,
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
    vkCreateFence(device, &fenceInfo, NULL, &drawFence);

    device.submit(cmd, imageAcquiredSemaphore, {}, drawFence);

    /* Make sure command buffer is finished before presenting */
    /* Amount of time, in nanoseconds, to wait for a command buffer to complete
     */
    auto FENCE_TIMEOUT = 100000000;
    VkResult res;
    do {
      res = vkWaitForFences(device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    } while (res == VK_TIMEOUT);
    assert(res == VK_SUCCESS);

    vuloxr::vk::CheckVkResult(swapchain.present(imageIndex));

    vkDestroyFence(device, drawFence, NULL);
    vkFreeCommandBuffers(device, cmd_pool, 1, &cmd);
  }

  vkDestroySemaphore(device, imageAcquiredSemaphore, NULL);
  vkDestroyPipelineCache(device, pipelineCache, NULL);
  vkDestroyCommandPool(device, cmd_pool, NULL);
}

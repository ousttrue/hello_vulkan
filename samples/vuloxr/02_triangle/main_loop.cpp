#include "../main_loop.h"
#include "vuloxr/vk.h"
#include <assert.h>
#include <vulkan/vulkan_core.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>

const char VS[] = {
#embed "shader.vert"
};

const char FS[] = {
#embed "shader.frag"
};

VkClearValue clear[] = {{
    .color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
}};

struct SwapchainRenderPass {};

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {
  //
  // pipeline
  //
  vuloxr::Logger::Info("convert glsl to spv and create shader module...");
  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, vuloxr::vk::glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, vuloxr::vk::glsl_fs_to_spv(FS), "main");

  auto pipelineLayout = vuloxr::vk::createEmptyPipelineLayout(device);

  auto [renderPass, depthStencil] = vuloxr::vk::createColorRenderPass(
      device, swapchain.createInfo.imageFormat);

  auto pipeline = vuloxr::vk::PipelineBuilder().create(
      device, renderPass, depthStencil, pipelineLayout,
      {
          vs.pipelineShaderStageCreateInfo,
          fs.pipelineShaderStageCreateInfo,
      },
      {}, {}, {}, {}, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  //
  // swapchain
  //
  vuloxr::vk::SwapchainNoDepthFramebufferList images(
      device, swapchain.createInfo.imageFormat,
      physicalDevice.graphicsFamilyIndex);

  vuloxr::vk::AcquireSemaphorePool semaphorePool(device);

  while (auto state = windowLoopOnce()) {
    auto acquireSemaphore = semaphorePool.getOrCreate();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);

    if (images.empty()) {
      images.reset(renderPass, swapchain.createInfo.imageExtent,
                   swapchain.images);
    }
    auto image = &images[acquired.imageIndex];

    semaphorePool.resetFenceAndMakePairSemaphore(image->submitFence,
                                                 acquireSemaphore);

    {
      vuloxr::vk::RenderPassRecording recording(
          image->commandBuffer, nullptr, pipeline.renderPass,
          image->framebuffer, swapchain.createInfo.imageExtent, clear);
      recording.draw(pipeline.graphicsPipeline, nullptr, 3);
    }

    vuloxr::vk::CheckVkResult(
        device.submit(image->commandBuffer, acquireSemaphore,
                      image->submitSemaphore, image->submitFence));
    vuloxr::vk::CheckVkResult(
        swapchain.present(acquired.imageIndex, image->submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}

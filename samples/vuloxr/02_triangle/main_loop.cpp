#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "vuloxr/vk.h"
#include <assert.h>
#include <vulkan/vulkan_core.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>

const char VS[] = {
#embed "shader.vert"
};

const char FS[] = {
#embed "shader.frag"
};

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {
  //
  // pipeline
  //
  vuloxr::Logger::Info("convert glsl to spv and create shader module...");
  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, glsl_fs_to_spv(FS), "main");

  auto pipelineLayout = vuloxr::vk::createEmptyPipelineLayout(device);

  auto renderPass = vuloxr::vk::createColorRenderPass(
      device, swapchain.createInfo.imageFormat);

  auto pipeline = vuloxr::vk::PipelineBuilder().create(
      device, renderPass, pipelineLayout,
      {
          vs.pipelineShaderStageCreateInfo,
          fs.pipelineShaderStageCreateInfo,
      },
      {}, {}, {}, {}, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  //
  // swapchain
  //

  std::vector<std::shared_ptr<vuloxr::vk::SwapchainFramebuffer>> images(
      swapchain.images.size());

  vuloxr::vk::FlightManager flightManager(device, swapchain.images.size());
  vuloxr::vk::CommandBufferPool pool(device, physicalDevice.graphicsFamilyIndex,
                                     swapchain.images.size());

  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);

    auto image = images[acquired.imageIndex];
    if (!image) {
      image = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
          device, acquired.image, swapchain.createInfo.imageExtent,
          swapchain.createInfo.imageFormat, pipeline.renderPass);
      images[acquired.imageIndex] = image;
    }

    auto [index, flight] = flightManager.sync(acquireSemaphore);
    auto cmd = pool.commandBuffers[index];
    vkResetCommandBuffer(cmd, 0);

    {
      VkClearValue clearValue = {
          .color =
              {
                  .float32 = {0.0f, 0.0f, 0.0f, 1.0f},
              },
      };
      vuloxr::vk::RenderPassRecording recording(
          cmd, nullptr, pipeline.renderPass, image->framebuffer,
          swapchain.createInfo.imageExtent, &clearValue, 1);
      recording.draw(pipeline.graphicsPipeline, nullptr, 3);
    }

    vuloxr::vk::CheckVkResult(device.submit(
        cmd, acquireSemaphore, flight.submitSemaphore, flight.submitFence));
    vuloxr::vk::CheckVkResult(
        swapchain.present(acquired.imageIndex, flight.submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}

#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "vko/vko.h"
#include "vko/vko_pipeline.h"
#include <assert.h>
#include <vulkan/vulkan_core.h>

const char VS[] = {
#embed "shader.vert"
};

const char FS[] = {
#embed "shader.frag"
};

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device) {
  //
  // pipeline
  //
  vko::Logger::Info("convert glsl to spv and create shader module...");
  auto vs =
      vko::ShaderModule::createVertexShader(device, glsl_vs_to_spv(VS), "main");
  auto fs = vko::ShaderModule::createFragmentShader(device, glsl_fs_to_spv(FS),
                                                    "main");

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pushConstantRangeCount = 0,
  };
  VkPipelineLayout pipelineLayout;
  vko::VKO_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                        &pipelineLayout));

  auto renderPass = vko::createColorRenderPass(
      device, surface.chooseSwapSurfaceFormat().format);

  auto pipeline = vko::PipelineBuilder().create(
      device, renderPass, pipelineLayout,
      {
          vs.pipelineShaderStageCreateInfo,
          fs.pipelineShaderStageCreateInfo,
      },
      {}, {}, {}, {}, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  //
  // swapchain
  //
  vko::Swapchain swapchain(device);
  swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex);

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> images(
      swapchain.images.size());

  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   swapchain.images.size());

  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);

    auto image = images[acquired.imageIndex];
    if (!image) {
      image = std::make_shared<vko::SwapchainFramebuffer>(
          device, acquired.image, swapchain.createInfo.imageExtent,
          swapchain.createInfo.imageFormat, pipeline.renderPass);
      images[acquired.imageIndex] = image;
    }

    auto [cmd, flight, oldSemaphore] = flightManager.sync(acquireSemaphore);
    if (oldSemaphore != VK_NULL_HANDLE) {
      flightManager.reuseSemaphore(oldSemaphore);
    }
    vkResetCommandBuffer(cmd, 0);

    {
      VkClearValue clearColor = {
          .color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
      };
      vko::CommandBufferRecording recording(
          cmd, pipeline.renderPass, pipeline.graphicsPipeline,
          image->framebuffer, swapchain.createInfo.imageExtent, clearColor);
      recording.draw(3);
    }

    vko::VKO_CHECK(device.submit(cmd, acquireSemaphore, flight.submitSemaphore,
                                 flight.submitFence));
    vko::VKO_CHECK(
        swapchain.present(acquired.imageIndex, flight.submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}

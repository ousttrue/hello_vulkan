#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "vko/vko.h"
#include "vko/vko_pipeline.h"
#include <assert.h>
#include <vulkan/vulkan_core.h>

auto VS = R"(#version 450

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
)";

auto FS = R"(#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device) {
  //
  // pipeline
  //
  vko::Logger::Info("convert glsl to spv and create shader module...");
  auto vsSpv = glsl_vs_to_spv(VS);
  vko::ShaderModule vs(device, vko::createShaderModule(device, vsSpv),
                       VK_SHADER_STAGE_VERTEX_BIT, "main");
  assert(vs != VK_NULL_HANDLE);

  auto fsSpv = glsl_fs_to_spv(FS);
  vko::ShaderModule fs(device, vko::createShaderModule(device, fsSpv),
                       VK_SHADER_STAGE_FRAGMENT_BIT, "main");
  assert(fs != VK_NULL_HANDLE);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pushConstantRangeCount = 0,
  };
  VkPipelineLayout pipelineLayout;
  VKO_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                   &pipelineLayout));

  auto pipeline = vko::createSimpleGraphicsPipeline(
      device, surface.chooseSwapSurfaceFormat().format, vs, fs, pipelineLayout);

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
          cmd, pipeline.renderPass, image->framebuffer,
          swapchain.createInfo.imageExtent, clearColor);
      recording.draw(pipeline.graphicsPipeline, 3);
    }

    VKO_CHECK(device.submit(cmd, acquireSemaphore, flight.submitSemaphore,
                            flight.submitFence));
    VKO_CHECK(swapchain.present(acquired.imageIndex, flight.submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}

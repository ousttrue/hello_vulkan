#include "../main_loop.h"

#include <vuloxr/vk.h>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>

const char VS[] = {
#embed "triangle.vert"
    , 0, 0, 0, 0};

const char FS[] = {
#embed "triangle.frag"
    , 0, 0, 0, 0};

struct Vertex {
  float position[4];
  float color[4];
};

// A simple counter-clockwise triangle.
// We specify the positions directly in clip space.
static const Vertex data[] = {
    {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
    },
    {
        {-0.5f, +0.5f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
    },
    {
        {+0.5f, -0.5f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    },
};

struct MapMemory {};

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  vuloxr::vk::VertexBuffer mesh{
      .bindings = {{
          .binding = 0,
          .stride = sizeof(Vertex),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      }},
      .attributes = {
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = 0,
          },
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
              .offset = offsetof(Vertex, color), // 4 * sizeof(float),
          },
      }};
  mesh.allocate(physicalDevice, device, std::span<const Vertex>(data));

  auto [renderPass, depthStencil] = vuloxr::vk::createColorRenderPass(
      device, swapchain.createInfo.imageFormat);

  auto pipelineLayout = vuloxr::vk::createEmptyPipelineLayout(device);

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, vuloxr::vk::glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, vuloxr::vk::glsl_fs_to_spv(FS), "main");

  vuloxr::vk::PipelineBuilder builder;
  builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  auto pipeline =
      builder.create(device, renderPass, depthStencil, pipelineLayout,
                     {
                         vs.pipelineShaderStageCreateInfo,
                         fs.pipelineShaderStageCreateInfo,
                     },
                     mesh.bindings, mesh.attributes, {}, {},
                     {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  vuloxr::vk::SwapchainNoDepthFramebufferList backbuffers(
      device, swapchain.createInfo.imageFormat,
      physicalDevice.graphicsFamilyIndex);
  backbuffers.reset(renderPass, swapchain.createInfo.imageExtent,
                    swapchain.images);
  vuloxr::vk::AcquireSemaphorePool semaphorePool(device);

  vuloxr::FrameCounter counter;
  while (auto state = windowLoopOnce()) {
    auto acquireSemaphore = semaphorePool.getOrCreate();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);

    if (res == VK_SUCCESS) {
      auto backbuffer = &backbuffers[acquired.imageIndex];
      semaphorePool.resetFenceAndMakePairSemaphore(backbuffer->submitFence,
                                                   acquireSemaphore);

      {
        VkClearValue clear[] = {{
            .color =
                {
                    .float32 = {0.1f, 0.1f, 0.2f, 1.0f},

                },
        }};
        vuloxr::vk::RenderPassRecording recording(
            backbuffer->commandBuffer, nullptr, pipeline.renderPass,
            backbuffer->framebuffer, swapchain.createInfo.imageExtent, clear);
        mesh.draw(backbuffer->commandBuffer, pipeline);
      }

      vuloxr::vk::CheckVkResult(
          device.submit(backbuffer->commandBuffer, acquireSemaphore,
                        backbuffer->submitSemaphore, backbuffer->submitFence));

      res = swapchain.present(acquired.imageIndex, backbuffer->submitSemaphore);
    } else {
      semaphorePool.reuse(acquireSemaphore);
    }

    if (res == VK_SUCCESS) {
      counter.frameEnd();
    } else {
      if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        vuloxr::Logger::Warn("VK_ERROR_OUT_OF_DATE_KHR || VK_SUBOPTIMAL_KHR");
        vkQueueWaitIdle(swapchain.presentQueue);
        swapchain.create();
        // TODO: recreate renderPass
        backbuffers.reset(renderPass, swapchain.createInfo.imageExtent,
                          swapchain.images);
        continue;
      }
      // throw
      vuloxr::vk::CheckVkResult(res);
    }
  }

  vkDeviceWaitIdle(device);
}

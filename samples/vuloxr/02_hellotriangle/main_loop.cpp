#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "vuloxr/vk.h"
#include "vuloxr/vk/buffer.h"
#include "vuloxr/vk/pipeline.h"
#include <vulkan/vulkan_core.h>

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

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  auto mesh = vuloxr::vk::VertexBuffer::create(
      device, sizeof(data), std::size(data),
      {{
          .binding = 0,
          .stride = sizeof(Vertex),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      }},
      {
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
      });

  auto meshMemory = physicalDevice.allocAndMapMemoryForBuffer(
      device, mesh.buffer, data, sizeof(data));

  auto renderPass = vuloxr::vk::createColorRenderPass(
      device, swapchain.createInfo.imageFormat);

  auto pipelineLayout = vuloxr::vk::createEmptyPipelineLayout(device);

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, glsl_fs_to_spv(FS), "main");

  vuloxr::vk::PipelineBuilder builder;
  builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  auto pipeline =
      builder.create(device, renderPass, pipelineLayout,
                     {
                         vs.pipelineShaderStageCreateInfo,
                         fs.pipelineShaderStageCreateInfo,
                     },
                     mesh.bindings, mesh.attributes, {}, {},
                     {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  std::vector<std::shared_ptr<vuloxr::vk::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  vuloxr::vk::FlightManager flightManager(device, swapchain.images.size());
  vuloxr::vk::CommandBufferPool pool(device, physicalDevice.graphicsFamilyIndex,
                                     swapchain.images.size());

  vuloxr::FrameCounter counter;
  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);

    if (res == VK_SUCCESS) {
      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        backbuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, renderPass);
        backbuffers[acquired.imageIndex] = backbuffer;
      }

      // All queue submissions get a fence that CPU will wait
      // on for synchronization purposes.
      auto [index, flight] = flightManager.sync(acquireSemaphore);
      auto cmd = pool.commandBuffers[index];

      {
        vuloxr::vk::RenderPassRecording recording(
            cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            pipeline.renderPass, backbuffer->framebuffer,
            swapchain.createInfo.imageExtent, {0.1f, 0.1f, 0.2f, 1.0f});
        mesh.draw(cmd, pipeline);
      }

      vuloxr::vk::CheckVkResult(device.submit(
          cmd, acquireSemaphore, flight.submitSemaphore, flight.submitFence));

      res = swapchain.present(acquired.imageIndex, flight.submitSemaphore);
    } else {
      flightManager.reuseSemaphore(acquireSemaphore);
    }

    if (res == VK_SUCCESS) {
      counter.frameEnd();
    } else {
      if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        vuloxr::Logger::Warn("VK_ERROR_OUT_OF_DATE_KHR || VK_SUBOPTIMAL_KHR");
        vkQueueWaitIdle(swapchain.presentQueue);
        swapchain.create();
        backbuffers.clear();
        backbuffers.resize(swapchain.images.size());
        continue;
      }
      // throw
      vuloxr::vk::CheckVkResult(res);
    }
  }

  vkDeviceWaitIdle(device);
}

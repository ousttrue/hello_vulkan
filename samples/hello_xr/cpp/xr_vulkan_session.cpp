#include "xr_vulkan_session.h"
#include "CubeScene.h"
#include <map>
#include <thread>

#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>
#include <vuloxr/vk/swapchain.h>
#include <vuloxr/xr/session.h>
#include <vuloxr/xr/swapchain.h>

char VertexShaderGlsl[] = {
#embed "shader.vert"
    , 0};
static_assert(sizeof(VertexShaderGlsl), "VertexShaderGlsl");

char FragmentShaderGlsl[] = {
#embed "shader.frag"
    , 0};
static_assert(sizeof(FragmentShaderGlsl), "FragmentShaderGlsl");

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  VkCommandPool commandPool = VK_NULL_HANDLE;

  struct RenderTarget {
    std::shared_ptr<vuloxr::vk::SwapchainFramebuffer> framebuffer;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vuloxr::vk::Fence execFence;
  };
  std::map<VkImage, RenderTarget> framebufferMap;

  ViewRenderer(VkDevice _device, uint32_t queueFamilyIndex) : device(_device) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    vuloxr::vk::CheckVkResult(vkCreateCommandPool(this->device, &cmdPoolInfo,
                                                  nullptr, &this->commandPool));
  }

  ~ViewRenderer() {
    for (auto it = this->framebufferMap.begin();
         it != this->framebufferMap.end(); ++it) {
      vkFreeCommandBuffers(this->device, this->commandPool, 1,
                           &it->second.commandBuffer);
    }
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
  }

  void render(const vuloxr::vk::PhysicalDevice &physicalDevice,
              //
              VkRenderPass renderPass, VkPipelineLayout pipelineLayout,
              VkPipeline pipeline,
              //
              VkImage image, VkExtent2D extent, VkFormat colorFormat,
              VkFormat depthFormat, VkSampleCountFlagBits sampleCountFlagBits,
              //
              const VkClearColorValue &clearColor, VkBuffer vertices,
              VkBuffer indices, uint32_t drawCount,
              std::span<const Mat4> cubes) {
    auto found = this->framebufferMap.find(image);
    if (found == this->framebufferMap.end()) {

      auto framebuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
          physicalDevice, this->device, image, extent, colorFormat, renderPass,
          depthFormat, sampleCountFlagBits);

      VkCommandBufferAllocateInfo cmd{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = this->commandPool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      VkCommandBuffer commandBuffer;
      vuloxr::vk::CheckVkResult(
          vkAllocateCommandBuffers(this->device, &cmd, &commandBuffer));

      this->framebufferMap[image] = {
          .framebuffer = framebuffer,
          .commandBuffer = commandBuffer,
          .execFence = vuloxr::vk::Fence(this->device, true),
      };
      found = this->framebufferMap.find(image);
      // found = this->framebufferMap.insert(std::make_pair(image, rt)).first;

      {
        vuloxr::vk::CommandScope(commandBuffer)
            .transitionDepthLayout(
                found->second.framebuffer->depth.image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      }

      VkSubmitInfo submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .commandBufferCount = 1,
          .pCommandBuffers = &commandBuffer,
      };
      vuloxr::vk::CheckVkResult(
          vkQueueSubmit(this->queue, 1, &submitInfo, nullptr));
      vkQueueWaitIdle(this->queue);
    }

    // Waiting on a not-in-flight command buffer is a no-op
    found->second.execFence.wait();
    found->second.execFence.reset();

    vuloxr::vk::CheckVkResult(
        vkResetCommandBuffer(found->second.commandBuffer, 0));

    {
      VkClearValue clearValues[] = {
          {.color = clearColor},
          {.depthStencil = {.depth = 1.0f, .stencil = 0}},
      };
      vuloxr::vk::RenderPassRecording recording(
          found->second.commandBuffer, pipelineLayout, renderPass,
          found->second.framebuffer->framebuffer, extent, clearValues,
          std::size(clearValues));

      vkCmdBindPipeline(found->second.commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

      // Bind index and vertex buffers
      vkCmdBindIndexBuffer(found->second.commandBuffer, indices, 0,
                           VK_INDEX_TYPE_UINT16);
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(found->second.commandBuffer, 0, 1, &vertices,
                             &offset);

      // Render each cube
      for (auto &cube : cubes) {
        vkCmdPushConstants(found->second.commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cube),
                           &cube.m);

        // Draw the cube.
        vkCmdDrawIndexed(found->second.commandBuffer, drawCount, 1, 0, 0, 0);
      }
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &found->second.commandBuffer,
    };
    vuloxr::vk::CheckVkResult(
        vkQueueSubmit(this->queue, 1, &submitInfo, found->second.execFence));
  }
};

void xr_vulkan_session(const std::function<bool(bool)> &runLoop,
                       XrInstance instance, XrSystemId systemId,
                       XrSession session, XrSpace appSpace,
                       //
                       VkFormat viewFormat,
                       const vuloxr::vk::PhysicalDevice &physicalDevice,
                       uint32_t queueFamilyIndex, VkDevice device,
                       //
                       VkClearColorValue clearColor,
                       XrEnvironmentBlendMode blendMode,
                       XrViewConfigurationType viewConfigurationType) {

  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
  vuloxr::vk::VertexBuffer vertices{
      .bindings =
          {
              {
                  .binding = 0,
                  .stride = static_cast<uint32_t>(sizeof(Vertex)),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributes = {
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, Position),
          },
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, Color),
          },
      }};
  vertices.allocate(physicalDevice, device,
                    std::span<const Vertex>(c_cubeVertices));

  vuloxr::vk::IndexBuffer indices(
      physicalDevice, device, std::span<const unsigned short>(c_cubeIndices));

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);

  // Create resources for each view.
  using VulkanSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageVulkan2KHR>;
  std::vector<std::shared_ptr<VulkanSwapchain>> swapchains;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // XrSwapchain
    auto swapchain = std::make_shared<VulkanSwapchain>(
        session, i, stereoscope.viewConfigurations[i], viewFormat,
        XrSwapchainImageVulkan2KHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    swapchains.push_back(swapchain);
  }

  // pieline
  auto pipelineLayout = vuloxr::vk::createPipelineLayoutWithConstantSize(
      device, sizeof(float) * 16);
  auto [renderPass, depthStencil] =
      vuloxr::vk::createColorDepthRenderPass(device, viewFormat, depthFormat);

  auto vertexSPIRV = vuloxr::vk::glsl_vs_to_spv(VertexShaderGlsl);
  assert(vertexSPIRV.size());
  auto vs =
      vuloxr::vk::ShaderModule::createVertexShader(device, vertexSPIRV, "main");

  auto fragmentSPIRV = vuloxr::vk::glsl_fs_to_spv(FragmentShaderGlsl);
  assert(fragmentSPIRV.size());
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, fragmentSPIRV, "main");

  std::vector<VkPipelineShaderStageCreateInfo> stages = {
      vs.pipelineShaderStageCreateInfo,
      fs.pipelineShaderStageCreateInfo,
  };

  vuloxr::vk::PipelineBuilder builder;
  auto pipeline =
      builder.create(device, renderPass, depthStencil, pipelineLayout, stages,
                     vertices.bindings, vertices.attributes, {}, {},
                     {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  ViewRenderer renderer(device, queueFamilyIndex);

  // mainloop
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::InputState input(instance, session);

  CubeScene scene(session);

  while (runLoop(state.m_sessionRunning)) {
    state.PollEvents();
    if (state.m_exitRenderLoop) {
      break;
    }

    if (!state.m_sessionRunning) {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      continue;
    }

    input.PollActions();

    auto frameState = vuloxr::xr::beginFrame(session);
    vuloxr::xr::LayerComposition composition(appSpace, blendMode);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {
        // XrCompositionLayerProjection

        scene.beginFrame();
        for (XrSpace visualizedSpace : scene.spaces) {
          auto [res, location] = vuloxr::xr::locate(
              appSpace, frameState.predictedDisplayTime, visualizedSpace);
          if (vuloxr::xr::locationIsValid(res, location)) {
            scene.cubes.push_back({location.pose, {0.25f, 0.25f, 0.25f}});
          }
        }

        for (auto &hand : input.hands) {
          auto [res, location] = vuloxr::xr::locate(
              appSpace, frameState.predictedDisplayTime, hand.space);
          auto s = hand.scale * 0.1f;
          if (vuloxr::xr::locationIsValid(res, location)) {
            scene.cubes.push_back({location.pose, {s, s, s}});
          }
        }

        for (uint32_t i = 0; i < stereoscope.views.size(); ++i) {
          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto [_, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          VkExtent2D extent = {
              swapchain->swapchainCreateInfo.width,
              swapchain->swapchainCreateInfo.height,
          };
          renderer.render(
              physicalDevice,
              //
              renderPass, pipelineLayout, pipeline,
              //
              image.image, extent,
              (VkFormat)swapchain->swapchainCreateInfo.format, depthFormat,
              (VkSampleCountFlagBits)swapchain->swapchainCreateInfo.sampleCount,
              clearColor, vertices.buffer, indices.buffer, indices.drawCount,
              scene.endFrame(projectionLayer.pose, projectionLayer.fov));

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers,
                         blendMode);
  }

  vkDeviceWaitIdle(device);
}

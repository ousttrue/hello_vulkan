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

struct ViewRenderer : vuloxr::NonCopyable {
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;

  struct RenderTarget {
    std::shared_ptr<vuloxr::vk::SwapchainFramebuffer> framebuffer;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vuloxr::vk::Fence execFence;
  };
  std::vector<std::shared_ptr<RenderTarget>> framebuffers;

  // ViewRenderer() {}

  ViewRenderer(const vuloxr::vk::PhysicalDevice &physicalDevice,
               VkDevice _device, uint32_t queueFamilyIndex,
               std::span<const XrSwapchainImageVulkan2KHR> images,
               VkExtent2D extent, VkFormat colorFormat, VkFormat depthFormat,
               VkSampleCountFlagBits sampleCountFlagBits,
               VkRenderPass renderPass)
      : device(_device), framebuffers(images.size()) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    vuloxr::vk::CheckVkResult(vkCreateCommandPool(this->device, &cmdPoolInfo,
                                                  nullptr, &this->commandPool));

    for (int index = 0; index < images.size(); ++index) {
      auto found = std::make_shared<RenderTarget>();

      found->execFence = vuloxr::vk::Fence(this->device, true);
      found->framebuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
          physicalDevice, this->device, images[index].image, extent,
          colorFormat, renderPass, depthFormat, sampleCountFlagBits);

      VkCommandBufferAllocateInfo cmd{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = this->commandPool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      // VkCommandBuffer commandBuffer;
      vuloxr::vk::CheckVkResult(
          vkAllocateCommandBuffers(this->device, &cmd, &found->commandBuffer));

      this->framebuffers[index] = found;

      {
        vuloxr::vk::CommandScope(found->commandBuffer)
            .transitionDepthLayout(
                found->framebuffer->depth.image, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
      }

      VkSubmitInfo submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .commandBufferCount = 1,
          .pCommandBuffers = &found->commandBuffer,
      };
      vuloxr::vk::CheckVkResult(
          vkQueueSubmit(this->queue, 1, &submitInfo, nullptr));
      vkQueueWaitIdle(this->queue);
    }
  }

  ~ViewRenderer() {
    for (auto it : this->framebuffers) {
      vkFreeCommandBuffers(this->device, this->commandPool, 1,
                           &it->commandBuffer);
    }
    if (this->commandPool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(this->device, this->commandPool, nullptr);
    }
  }

  ViewRenderer(ViewRenderer &&rhs) {
    this->device = rhs.device;
    this->queue = rhs.queue;
    this->commandPool = rhs.commandPool;
    rhs.commandPool = VK_NULL_HANDLE;
    std::swap(this->framebuffers, rhs.framebuffers);
  }

  ViewRenderer &operator=(ViewRenderer &&rhs) {
    this->device = rhs.device;
    this->queue = rhs.queue;
    this->commandPool = rhs.commandPool;
    rhs.commandPool = VK_NULL_HANDLE;
    std::swap(this->framebuffers, rhs.framebuffers);
    return *this;
  }

  void render(
      //
      VkRenderPass renderPass, VkPipelineLayout pipelineLayout,
      VkPipeline pipeline,
      //
      uint32_t index, VkExtent2D extent,
      //
      const VkClearColorValue &clearColor, VkBuffer vertices, VkBuffer indices,
      uint32_t drawCount, std::span<const Mat4> cubes) {}
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

  // Create resources for each view.
  using VulkanSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageVulkan2KHR>;
  std::vector<std::shared_ptr<VulkanSwapchain>> swapchains;
  std::vector<ViewRenderer> renderers;

  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // XrSwapchain
    auto swapchain = std::make_shared<VulkanSwapchain>(
        session, i, stereoscope.viewConfigurations[i], viewFormat,
        XrSwapchainImageVulkan2KHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    swapchains.push_back(swapchain);

    VkExtent2D extent = {
        swapchain->swapchainCreateInfo.width,
        swapchain->swapchainCreateInfo.height,
    };

    renderers.push_back(ViewRenderer(
        physicalDevice, device, queueFamilyIndex, swapchain->swapchainImages,
        extent, viewFormat, depthFormat,
        (VkSampleCountFlagBits)swapchain->swapchainCreateInfo.sampleCount,
        renderPass));
  }

  CubeScene scene(session);

  // mainloop
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::InputState input(instance, session);
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
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          VkExtent2D extent = {
              swapchain->swapchainCreateInfo.width,
              swapchain->swapchainCreateInfo.height,
          };

          auto cubes =
              scene.endFrame(projectionLayer.pose, projectionLayer.fov);

          auto found = renderers[i].framebuffers[index];

          // Waiting on a not-in-flight command buffer is a no-op
          found->execFence.wait();
          found->execFence.reset();

          vuloxr::vk::CheckVkResult(
              vkResetCommandBuffer(found->commandBuffer, 0));

          {
            VkClearValue clearValues[] = {
                {.color = clearColor},
                {.depthStencil = {.depth = 1.0f, .stencil = 0}},
            };
            vuloxr::vk::RenderPassRecording recording(
                found->commandBuffer, pipelineLayout, renderPass,
                found->framebuffer->framebuffer, extent, clearValues,
                std::size(clearValues));

            vkCmdBindPipeline(found->commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            // Bind index and vertex buffers
            vkCmdBindIndexBuffer(found->commandBuffer, indices.buffer, 0,
                                 VK_INDEX_TYPE_UINT16);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(found->commandBuffer, 0, 1,
                                   &vertices.buffer.buffer, &offset);

            // Render each cube
            for (auto &cube : cubes) {
              vkCmdPushConstants(found->commandBuffer, pipelineLayout,
                                 VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cube),
                                 &cube.m);

              // Draw the cube.
              vkCmdDrawIndexed(found->commandBuffer, indices.drawCount, 1, 0, 0,
                               0);
            }
          }

          VkSubmitInfo submitInfo{
              .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
              .commandBufferCount = 1,
              .pCommandBuffers = &found->commandBuffer,
          };
          vuloxr::vk::CheckVkResult(
              vkQueueSubmit(renderers[i].queue, 1, &submitInfo, found->execFence));

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

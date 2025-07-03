#include "../xr_main_loop.h"
#include "CubeScene.h"
#include "xr_linear.h"
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
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vuloxr::vk::Fence execFence;
  };
  std::vector<std::shared_ptr<RenderTarget>> renderTargets;
  vuloxr::vk::SwapchainIsolatedDepthFramebufferList framebuffers;
  std::vector<DirectX::XMFLOAT4X4> matrices;

  ViewRenderer(const vuloxr::vk::PhysicalDevice &physicalDevice,
               VkDevice _device, uint32_t queueFamilyIndex,
               std::span<VkImage> images, VkExtent2D extent,
               VkFormat colorFormat, VkFormat depthFormat,
               VkSampleCountFlagBits sampleCountFlagBits,
               VkRenderPass renderPass)
      : device(_device), renderTargets(images.size()),
        framebuffers(_device, colorFormat, depthFormat, sampleCountFlagBits) {
    framebuffers.reset(physicalDevice, renderPass, extent, images);
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    vuloxr::vk::CheckVkResult(vkCreateCommandPool(this->device, &cmdPoolInfo,
                                                  nullptr, &this->commandPool));

    for (int index = 0; index < images.size(); ++index) {
      auto rt = std::make_shared<RenderTarget>();

      rt->execFence = vuloxr::vk::Fence(this->device, true);

      VkCommandBufferAllocateInfo cmd{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = this->commandPool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };
      vuloxr::vk::CheckVkResult(
          vkAllocateCommandBuffers(this->device, &cmd, &rt->commandBuffer));

      this->renderTargets[index] = rt;
    }
  }

  ~ViewRenderer() {
    if (this->commandPool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(this->device, this->commandPool, nullptr);
    }
  }

  ViewRenderer(ViewRenderer &&rhs) : framebuffers(std::move(rhs.framebuffers)) {
    this->device = rhs.device;
    this->queue = rhs.queue;
    this->commandPool = rhs.commandPool;
    rhs.commandPool = VK_NULL_HANDLE;
    std::swap(this->renderTargets, rhs.renderTargets);
  }

  ViewRenderer &operator=(ViewRenderer &&rhs) {
    this->device = rhs.device;
    this->queue = rhs.queue;
    this->commandPool = rhs.commandPool;
    rhs.commandPool = VK_NULL_HANDLE;
    std::swap(this->renderTargets, rhs.renderTargets);
    this->framebuffers = std::move(rhs.framebuffers);
    return *this;
  }

  void render(uint32_t index, VkRenderPass renderPass,
              VkPipelineLayout pipelineLayout, VkPipeline pipeline,
              VkExtent2D extent, std::span<const VkClearValue> clearValues,
              VkBuffer vertices, VkBuffer indices, uint32_t drawCount) {
    auto framebuffer = &this->framebuffers[index];
    auto rt = this->renderTargets[index];

    // Waiting on a not-in-flight command buffer is a no-op
    rt->execFence.wait();
    rt->execFence.reset();

    vuloxr::vk::CheckVkResult(vkResetCommandBuffer(rt->commandBuffer, 0));

    {
      vuloxr::vk::RenderPassRecording recording(
          rt->commandBuffer, pipelineLayout, renderPass,
          framebuffer->framebuffer, extent, clearValues);

      vkCmdBindPipeline(rt->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipeline);

      // Bind index and vertex buffers
      vkCmdBindIndexBuffer(rt->commandBuffer, indices, 0, VK_INDEX_TYPE_UINT16);
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(rt->commandBuffer, 0, 1, &vertices, &offset);

      // Render each cube
      for (auto &m : this->matrices) {
        vkCmdPushConstants(rt->commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m.m);

        // Draw the cube.
        vkCmdDrawIndexed(rt->commandBuffer, drawCount, 1, 0, 0, 0);
      }
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &rt->commandBuffer,
    };
    vuloxr::vk::CheckVkResult(
        vkQueueSubmit(this->queue, 1, &submitInfo, rt->execFence));
  }
};

void xr_main_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
                  XrSystemId systemId, XrSession session, XrSpace appSpace,
                  const XrGraphics &graphics, XrEnvironmentBlendMode blendMode,
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
  vertices.allocate(graphics.physicalDevice, graphics.device,
                    std::span<const Vertex>(c_cubeVertices));

  vuloxr::vk::IndexBuffer indices(
      graphics.physicalDevice, graphics.device,
      std::span<const unsigned short>(c_cubeIndices));

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);

  // pieline
  auto pipelineLayout = vuloxr::vk::createPipelineLayoutWithConstantSize(
      graphics.device, sizeof(float) * 16);
  auto [renderPass, depthStencil] = vuloxr::vk::createColorDepthRenderPass(
      graphics.device, graphics.format, depthFormat);

  auto vertexSPIRV = vuloxr::vk::glsl_vs_to_spv(VertexShaderGlsl);
  assert(vertexSPIRV.size());
  auto vs = vuloxr::vk::ShaderModule::createVertexShader(graphics.device,
                                                         vertexSPIRV, "main");

  auto fragmentSPIRV = vuloxr::vk::glsl_fs_to_spv(FragmentShaderGlsl);
  assert(fragmentSPIRV.size());
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      graphics.device, fragmentSPIRV, "main");

  std::vector<VkPipelineShaderStageCreateInfo> stages = {
      vs.pipelineShaderStageCreateInfo,
      fs.pipelineShaderStageCreateInfo,
  };

  vuloxr::vk::PipelineBuilder builder;
  auto pipeline =
      builder.create(graphics.device, renderPass, depthStencil, pipelineLayout,
                     stages, vertices.bindings, vertices.attributes, {}, {},
                     {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  // Create resources for each view.
  using VulkanSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageVulkan2KHR>;
  std::vector<std::shared_ptr<VulkanSwapchain>> swapchains;
  std::vector<ViewRenderer> renderers;

  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // XrSwapchain
    auto swapchain = std::make_shared<VulkanSwapchain>(
        session, i, stereoscope.viewConfigurations[i], graphics.format,
        XrSwapchainImageVulkan2KHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    swapchains.push_back(swapchain);

    VkExtent2D extent = {
        swapchain->swapchainCreateInfo.width,
        swapchain->swapchainCreateInfo.height,
    };

    std::vector<VkImage> images;
    for (auto image : swapchain->swapchainImages) {
      images.push_back(image.image);
    }
    renderers.push_back(ViewRenderer(
        graphics.physicalDevice, graphics.device, graphics.queueFamilyIndex,
        images, extent, graphics.format, depthFormat,
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
        scene.clear();
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

          // Compute the view-projection transform. Note all matrixes (including
          // OpenXR's) are column-major, right-handed.
          XrMatrix4x4f proj;
          XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN,
                                           projectionLayer.fov, 0.05f, 100.0f);
          XrMatrix4x4f toView;
          XrMatrix4x4f_CreateFromRigidTransform(&toView, &projectionLayer.pose);
          XrMatrix4x4f view;
          XrMatrix4x4f_InvertRigidBody(&view, &toView);
          XrMatrix4x4f vp;
          XrMatrix4x4f_Multiply(&vp, &proj, &view);

          scene.calcMatrix(*((DirectX::XMFLOAT4X4 *)&vp),
                           renderers[i].matrices);

          VkClearValue clearValues[] = {
              {.color = graphics.clearColor},
              {.depthStencil = {.depth = 1.0f, .stencil = 0}},
          };

          renderers[i].render(index, renderPass, pipelineLayout, pipeline,
                              extent, clearValues, vertices.buffer,
                              indices.buffer, indices.drawCount);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers,
                         blendMode);
  }

  vkDeviceWaitIdle(graphics.device);
}

#include "../xr_main_loop.h"

// #include "util_shader.h"
#include <DirectXMath.h>

#include <vuloxr/xr/session.h>
#include <vuloxr/xr/swapchain.h>

#include <thread>

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
using GraphicsSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLESKHR>;
const XrSwapchainImageOpenGLESKHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,
};
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
using GraphicsSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLKHR>;
const XrSwapchainImageOpenGLKHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,
};
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
using GraphicsSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageVulkan2KHR>;
XrSwapchainImageVulkan2KHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR,
};
#endif

static XrVector2f s_vtx[] = {
    {-0.5f, 0.5f},  //
    {-0.5f, -0.5f}, //
    {0.5f, 0.5f},   //
};

static XrVector4f s_col[] = {
    {1.0f, 0.0f, 0.0f, 1.0f}, //
    {0.0f, 1.0f, 0.0f, 1.0f}, //
    {0.0f, 0.0f, 1.0f, 1.0f}, //
};

const char s_strVS[]{
#embed "shader.vert"
    ,
    0,
};

const char s_strFS[]{
#embed "shader.frag"
    ,
    0,
};

// static const shader_obj_t &getShader() {
//   static bool s_initialized = false;
//   static shader_obj_t s_sobj;
//   if (!s_initialized) {
//     generate_shader(&s_sobj, s_strVS, s_strFS);
//     s_initialized = true;
//     vuloxr::Logger::Info("generate_shader");
//   }
//   return s_sobj;
// }

// struct ViewRenderer {
//   std::vector<std::shared_ptr<RenderTarget>> backbuffers;
//
//   ViewRenderer(int width, int height, std::span<const uint32_t> images)
//       : backbuffers(images.size()) {
//     for (int i = 0; i < images.size(); ++i) {
//       this->backbuffers[i] =
//           std::make_shared<RenderTarget>(images[i], width, height);
//     }
//   }
//
//   std::shared_ptr<RenderTarget> operator[](uint32_t index) const {
//     return this->backbuffers[index];
//   }
// };

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

    // {
    //   vuloxr::vk::RenderPassRecording recording(
    //       rt->commandBuffer, pipelineLayout, renderPass,
    //       framebuffer->framebuffer, extent, clearValues);
    //
    //   vkCmdBindPipeline(rt->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                     pipeline);
    //
    //   // Bind index and vertex buffers
    //   vkCmdBindIndexBuffer(rt->commandBuffer, indices, 0, VK_INDEX_TYPE_UINT16);
    //   VkDeviceSize offset = 0;
    //   vkCmdBindVertexBuffers(rt->commandBuffer, 0, 1, &vertices, &offset);
    //
    //   // Render each cube
    //   for (auto &m : this->matrices) {
    //     vkCmdPushConstants(rt->commandBuffer, pipelineLayout,
    //                        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m.m);
    //
    //     // Draw the cube.
    //     vkCmdDrawIndexed(rt->commandBuffer, drawCount, 1, 0, 0, 0);
    //   }
    // }

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
                  std::span<const int64_t> formats,
                  //
                  const Graphics &graphics,
                  //
                  const XrColor4f &clearColor, XrEnvironmentBlendMode blendMode,
                  XrViewConfigurationType viewConfigurationType) {

  auto format = *vuloxr::vk::selectColorSwapchainFormat(formats);

  // auto viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  // swapchain
  std::vector<std::shared_ptr<GraphicsSwapchain>> swapchains;
  std::vector<ViewRenderer> renderers;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // XrSwapchain
    auto swapchain = std::make_shared<GraphicsSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format,
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
    // renderers.push_back(ViewRenderer(
    //     graphics.physicalDevice, graphics.device,
    //     graphics.physicalDevice.graphicsFamilyIndex, images, extent, format,
    //     depthFormat,
    //     (VkSampleCountFlagBits)swapchain->swapchainCreateInfo.sampleCount,
    //     renderPass));
  }

  // for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
  //   auto swapchain = std::make_shared<GraphicsSwapchain>(
  //       session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
  //   swapchains.push_back(swapchain);
  //
  //   auto &viewConf = stereoscope.viewConfigurations[i];
  //   std::vector<uint32_t> images;
  //   for (auto &image : swapchain->swapchainImages) {
  //     images.push_back(image.image);
  //   }
  //   auto renderer = std::make_shared<ViewRenderer>(
  //       swapchain->swapchainCreateInfo.width,
  //       swapchain->swapchainCreateInfo.height, images);
  //   renderers.push_back(renderer);
  // }

  // auto sobj = &getShader();
  //
  // Vbo positions;
  // positions.assign<XrVector2f>(s_vtx);
  // Vbo colors;
  // colors.assign<XrVector4f>(s_col);
  // Vao vao;
  // vao.assign(std::span<const Vao::AttributeLayout>({
  //     {
  //         .attribute = sobj->loc_vtx,
  //         .vbo = positions.id,
  //         .components = 2,
  //         .stride = 8,
  //         .offset = 0,
  //     },
  //     {
  //         .attribute = sobj->loc_clr,
  //         .vbo = colors.id,
  //         .components = 4,
  //         .stride = 16,
  //         .offset = 0,
  //     },
  // }));

  // mainloop
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

    auto frameState = vuloxr::xr::beginFrame(session);
    vuloxr::xr::LayerComposition composition(appSpace);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {

        for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
          auto swapchain = swapchains[i];
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);

          // {
          //   //  Render
          //   auto backbuffer = (*renderers[i])[index];
          //   backbuffer->beginFrame(swapchain->swapchainCreateInfo.width,
          //                          swapchain->swapchainCreateInfo.height,
          //                          clearColor);
          //
          //   glUseProgram(sobj->program);
          //
          //   vao.bind();
          //
          //   glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
          //
          //   vao.unbind();
          //   backbuffer->endFrame();
          // }

          composition.pushView(projectionLayer);

          swapchain->EndSwapchain();
        }
      }
    }

    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers);

  } // while
}

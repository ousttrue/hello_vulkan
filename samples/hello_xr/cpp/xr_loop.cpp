#include "xr_loop.h"
#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include "vko/vko.h"
#include "vkr/VulkanRenderer.h"

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  std::shared_ptr<VulkanRenderer> vulkanRenderer;
  vko::Fence execFence;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  ViewRenderer(VkDevice _device, uint32_t queueFamilyIndex)
      : device(_device), execFence(_device, true) {
    vkGetDeviceQueue(this->device, queueFamilyIndex, 0, &this->queue);

    VkCommandPoolCreateInfo cmdPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VKO_CHECK(vkCreateCommandPool(this->device, &cmdPoolInfo, nullptr,
                                  &this->commandPool));
    if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_POOL,
                                   (uint64_t)this->commandPool,
                                   "hello_xr command pool") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }

    VkCommandBufferAllocateInfo cmd{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VKO_CHECK(
        vkAllocateCommandBuffers(this->device, &cmd, &this->commandBuffer));
    if (SetDebugUtilsObjectNameEXT(device, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                   (uint64_t)this->commandBuffer,
                                   "hello_xr command buffer") != VK_SUCCESS) {
      throw std::runtime_error("SetDebugUtilsObjectNameEXT");
    }
  }

  ~ViewRenderer() {
    vkDestroyFence(this->device, execFence, nullptr);

    vkFreeCommandBuffers(this->device, this->commandPool, 1,
                         &this->commandBuffer);
    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
  }

  void render(VkImage image, const Vec4 &clearColor,
              const std::vector<Mat4> &matrices) {
    // Waiting on a not-in-flight command buffer is a no-op
    execFence.wait();
    execFence.reset();

    VKO_CHECK(vkResetCommandBuffer(this->commandBuffer, 0));

    VkCommandBufferBeginInfo cmdBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VKO_CHECK(vkBeginCommandBuffer(this->commandBuffer, &cmdBeginInfo));

    this->vulkanRenderer->RenderView(this->commandBuffer, image, clearColor,
                                     matrices);

    vkCmdEndRenderPass(this->commandBuffer);
    VKO_CHECK(vkEndCommandBuffer(this->commandBuffer));

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &this->commandBuffer,
    };
    VKO_CHECK(vkQueueSubmit(this->queue, 1, &submitInfo, execFence));
  }
};

void xr_loop(const std::function<bool()> &runLoop, const Options &options,
             const std::shared_ptr<OpenXrSession> &session,
             VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
             VkDevice device) {

  // Create resources for each view.
  auto config = session->GetSwapchainConfiguration();
  std::vector<std::shared_ptr<OpenXrSwapchain>> swapchains;
  // std::vector<std::shared_ptr<VulkanRenderer>> renderers;
  // std::vector<std::shared_ptr<CmdBuffer>> cmdBuffers;
  std::vector<std::shared_ptr<ViewRenderer>> views;

  for (uint32_t i = 0; i < config.Views.size(); i++) {
    auto ptr = std::make_shared<ViewRenderer>(device, queueFamilyIndex);
    views.push_back(ptr);

    // XrSwapchain
    auto swapchain = OpenXrSwapchain::Create(session->m_session, i,
                                             config.Views[i], config.Format);
    swapchains.push_back(swapchain);

    // VkPipeline... etc
    ptr->vulkanRenderer = std::make_shared<VulkanRenderer>(
        physicalDevice, device, queueFamilyIndex, swapchain->extent(),
        swapchain->format(), swapchain->sampleCountFlagBits());
  }

  // mainloop
  while (runLoop()) {

    auto frameState = session->BeginFrame();
    LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                 session->m_appSpace);

    if (frameState.shouldRender == XR_TRUE) {
      uint32_t viewCountOutput;
      if (session->LocateView(
              session->m_appSpace, frameState.predictedDisplayTime,
              options.Parsed.ViewConfigType, &viewCountOutput)) {
        // XrCompositionLayerProjection

        // update scene
        CubeScene scene;
        scene.addSpaceCubes(session->m_appSpace,
                            frameState.predictedDisplayTime,
                            session->m_visualizedSpaces);
        scene.addHandCubes(session->m_appSpace, frameState.predictedDisplayTime,
                           session->m_input);

        for (uint32_t i = 0; i < viewCountOutput; ++i) {

          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto info = swapchain->AcquireSwapchain(session->m_views[i]);
          composition.pushView(info.CompositionLayer);

          views[i]->render(info.Image, options.GetBackgroundClearColor(),
                           scene.CalcCubeMatrices(info.calcViewProjection()));

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    session->EndFrame(frameState.predictedDisplayTime, layers);
  }

  vkDeviceWaitIdle(device);
}

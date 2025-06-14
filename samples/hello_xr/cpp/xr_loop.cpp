#include "xr_loop.h"
#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include "vkr/CmdBuffer.h"
#include "vkr/VulkanRenderer.h"

struct ViewRenderer {
  std::shared_ptr<VulkanRenderer> vulkanRenderer;
  std::shared_ptr<CmdBuffer> cmdBuffer;

  void render(VkImage image, const Vec4 &clearColor,
              const std::vector<Mat4> &matrices) {
    this->cmdBuffer->Wait();
    this->cmdBuffer->Reset();
    this->cmdBuffer->Begin();

    this->vulkanRenderer->RenderView(this->cmdBuffer->buf, image, clearColor,
                                     matrices);

    vkCmdEndRenderPass(this->cmdBuffer->buf);
    this->cmdBuffer->End();
    this->cmdBuffer->Exec();
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
    auto ptr = std::make_shared<ViewRenderer>();
    views.push_back(ptr);

    // XrSwapchain
    auto swapchain = OpenXrSwapchain::Create(session->m_session, i,
                                             config.Views[i], config.Format);
    swapchains.push_back(swapchain);

    // VkPipeline... etc
    ptr->vulkanRenderer = std::make_shared<VulkanRenderer>(
        physicalDevice, device, queueFamilyIndex, swapchain->extent(),
        swapchain->format(), swapchain->sampleCountFlagBits());

    ptr->cmdBuffer = CmdBuffer::Create(device, queueFamilyIndex);
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

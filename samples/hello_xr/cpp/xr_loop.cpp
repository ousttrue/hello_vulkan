#include "xr_loop.h"
#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include "vkr/CmdBuffer.h"
#include "vkr/VulkanRenderer.h"

static void render_view(const std::shared_ptr<CmdBuffer> &cmdBuffer,
                        VkImage image, const Vec4 &clearColor,
                        const std::shared_ptr<VulkanRenderer> &renderer,
                        const std::vector<Mat4> &matrices) {
  cmdBuffer->Wait();
  cmdBuffer->Reset();
  cmdBuffer->Begin();

  renderer->RenderView(cmdBuffer->buf, image, clearColor, matrices);

  vkCmdEndRenderPass(cmdBuffer->buf);
  cmdBuffer->End();
  cmdBuffer->Exec();
}

void xr_loop(const std::function<bool()> &runLoop, const Options &options,
             const std::shared_ptr<OpenXrSession> &session,
             VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
             VkDevice device) {

  // Create resources for each view.
  auto config = session->GetSwapchainConfiguration();
  std::vector<std::shared_ptr<OpenXrSwapchain>> swapchains;
  std::vector<std::shared_ptr<VulkanRenderer>> renderers;
  std::vector<std::shared_ptr<CmdBuffer>> cmdBuffers;

  for (uint32_t i = 0; i < config.Views.size(); i++) {
    // XrSwapchain
    auto swapchain = OpenXrSwapchain::Create(session->m_session, i,
                                             config.Views[i], config.Format);
    swapchains.push_back(swapchain);

    // VkPipeline... etc
    auto renderer = std::make_shared<VulkanRenderer>(
        physicalDevice, device, queueFamilyIndex,
        VkExtent2D{swapchain->m_swapchainCreateInfo.width,
                   swapchain->m_swapchainCreateInfo.height},
        static_cast<VkFormat>(swapchain->m_swapchainCreateInfo.format),
        static_cast<VkSampleCountFlagBits>(
            swapchain->m_swapchainCreateInfo.sampleCount));
    renderers.push_back(renderer);

    auto cmdBuffer = CmdBuffer::Create(device, queueFamilyIndex);
    assert(cmdBuffer);
    cmdBuffers.push_back(cmdBuffer);
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

          render_view(cmdBuffers[i], info.Image,
                      options.GetBackgroundClearColor(), renderers[i],
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

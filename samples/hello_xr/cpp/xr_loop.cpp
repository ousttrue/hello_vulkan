#include "xr_loop.h"
#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_session.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include <vkr/vulkan_renderer.h>

void xr_loop(const std::function<std::tuple<bool, XrFrameState>()> &runLoop,
             const std::shared_ptr<OpenXrSession> &session,
             const Options &options, VkPhysicalDevice physicalDevice,
             uint32_t queueFamilyIndex, VkDevice device) {

  // Create resources for each view.
  auto config = session->GetSwapchainConfiguration();
  std::vector<std::shared_ptr<OpenXrSwapchain>> swapchains;
  std::vector<std::shared_ptr<VulkanRenderer>> renderers;
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
  }

  // mainloop
  while (true) {
    auto [run, frameState] = runLoop();
    if (!run) {
      break;
    }

    if (frameState.shouldRender != XR_TRUE) {
      continue;
    }

    uint32_t viewCountOutput;
    if (!session->LocateView(session->m_appSpace,
                             frameState.predictedDisplayTime,
                             options.Parsed.ViewConfigType, &viewCountOutput)) {
      continue;
    }

    LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                 session->m_appSpace);

    // update scene
    CubeScene scene;
    scene.addSpaceCubes(session->m_appSpace, frameState.predictedDisplayTime,
                        session->m_visualizedSpaces);
    scene.addHandCubes(session->m_appSpace, frameState.predictedDisplayTime,
                       session->m_input);

    for (uint32_t i = 0; i < viewCountOutput; ++i) {
      // XrCompositionLayerProjectionView(left / right)
      auto swapchain = swapchains[i];
      auto info = swapchain->AcquireSwapchain(session->m_views[i]);
      composition.pushView(info.CompositionLayer);

      {
        // render vulkan
        auto renderer = renderers[i];
        VkCommandBuffer cmd = renderer->BeginCommand();
        renderer->RenderView(cmd, info.Image, options.GetBackgroundClearColor(),
                             scene.CalcCubeMatrices(info.calcViewProjection()));
        renderer->EndCommand(cmd);
      }

      swapchain->EndSwapchain();
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    session->EndFrame(frameState.predictedDisplayTime, layers);
  }

  vkDeviceWaitIdle(device);
}

#include "../xr_main_loop.h"
#include "app_engine.h"
#include <vuloxr/xr/session.h>

#include <thread>

#include <vuloxr/gl.h>

struct ViewSwapchain {
  std::vector<std::shared_ptr<vuloxr::gl::RenderTarget>> backbuffers;

  ViewSwapchain(std::shared_ptr<GraphicsSwapchain> &swapchain) {
    for (auto &image : swapchain->swapchainImages) {
      this->backbuffers.push_back(std::make_shared<vuloxr::gl::RenderTarget>(
          image.image, swapchain->swapchainCreateInfo.width,
          swapchain->swapchainCreateInfo.height));
    }
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

  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  auto format = Graphics::selectColorSwapchainFormat(formats);
  std::vector<std::shared_ptr<GraphicsSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewSwapchain>> views;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GraphicsSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
    swapchains.push_back(swapchain);

    views.push_back(std::make_shared<ViewSwapchain>(swapchain));
  }

  AppEngine engine(instance, systemId, session, appSpace);

  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
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
    vuloxr::xr::LayerComposition composition(appSpace, blendMode);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {

        for (uint32_t i = 0; i < stereoscope.views.size(); ++i) {
          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          engine.RenderLayer(frameState.predictedDisplayTime, i,
                             projectionLayer,
                             views[i]->backbuffers[index]->fbo.id);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers,
                         blendMode);
  }
}

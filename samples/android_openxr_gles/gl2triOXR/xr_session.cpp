#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include "ViewRenderer.h"
#include "xr_session.h"

#include <thread>
#include <vuloxr/xr/session.h>

void xr_session(const std::function<bool(bool)> &runLoop, XrInstance instance,
                XrSystemId systemId, XrSession session, XrSpace appSpace) {

  auto viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  // swapchain
  using GLESSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLESKHR>;
  std::vector<std::shared_ptr<GLESSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> renderers;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GLESSwapchain>(
        session, i, stereoscope.viewConfigurations[i], GL_RGBA8,
        XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
    swapchains.push_back(swapchain);

    renderers.push_back(std::make_shared<ViewRenderer>());
  }

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
          auto [_, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);

          auto renderer = renderers[i];
          renderer->render(image.image,
                           projectionLayer.subImage.imageRect.extent.width,
                           projectionLayer.subImage.imageRect.extent.height);

          composition.pushView(projectionLayer);

          swapchain->EndSwapchain();
        }
      }
    }

    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers);

  } // while
}

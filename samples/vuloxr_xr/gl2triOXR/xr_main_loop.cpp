#include "../xr_main_loop.h"

#include <vuloxr/xr/session.h>

#include <thread>

const char VS[]{
#ifdef XR_USE_GRAPHICS_API_VULKAN
#embed "vk.vert"
#else
#embed "gl.vert"
#endif
    ,
    0,
};

const char FS[]{
#ifdef XR_USE_GRAPHICS_API_VULKAN
#embed "vk.frag"
#else
#embed "gl.frag"
#endif
    ,
    0,
};

struct Vertex {
  XrVector2f position;
  XrVector4f color;
};

static Vertex vertices[]{
    {{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
};

static VertexAttributeLayout layouts[]{
    {.components = 2, .offset = offsetof(Vertex, position)},
    {.components = 4, .offset = offsetof(Vertex, color)},
};

void xr_main_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
                  XrSystemId systemId, XrSession session, XrSpace appSpace,
                  std::span<const int64_t> formats, const Graphics &graphics,
                  const XrColor4f &clearColor, XrEnvironmentBlendMode blendMode,
                  XrViewConfigurationType viewConfigurationType) {

  auto format = Graphics::selectColorSwapchainFormat(formats);

  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  // swapchain
  std::vector<std::shared_ptr<GraphicsSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> renderers;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GraphicsSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
    swapchains.push_back(swapchain);

    auto r = std::make_shared<ViewRenderer>(&graphics, swapchain);

    r->initScene(VS, FS, layouts,
                 {
                     .data = vertices,
                     .stride = static_cast<uint32_t>(sizeof(Vertex)),
                     .drawCount = std::size(vertices),
                 });

    r->initSwapchain(swapchain->swapchainCreateInfo.width,
                     swapchain->swapchainCreateInfo.height,
                     swapchain->swapchainImages);

    renderers.push_back(r);
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
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          auto r = renderers[i];
          r->render(index, clearColor);

          swapchain->EndSwapchain();
        }
      }
    }

    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers);

  } // while
}

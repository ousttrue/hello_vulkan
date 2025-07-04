#include "../xr_main_loop.h"

#include "util_shader.h"

#include <vuloxr/xr/session.h>
#include <vuloxr/xr/swapchain.h>

#include <stdint.h>
#include <thread>
#include <unordered_map>

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
using GLESSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLESKHR>;
const XrSwapchainImageOpenGLESKHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,
};
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
using GLESSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLKHR>;
const XrSwapchainImageOpenGLKHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,
};
#endif

static float s_vtx[] = {
    -0.5f, 0.5f,  //
    -0.5f, -0.5f, //
    0.5f,  0.5f,  //
};

static float s_col[] = {
    1.0f, 0.0f, 0.0f, 1.0f, //
    0.0f, 1.0f, 0.0f, 1.0f, //
    0.0f, 0.0f, 1.0f, 1.0f, //
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

static const shader_obj_t &getShader() {
  static bool s_initialized = false;
  static shader_obj_t s_sobj;
  if (!s_initialized) {
    generate_shader(&s_sobj, s_strVS, s_strFS);
    s_initialized = true;
    vuloxr::Logger::Info("generate_shader");
  }
  return s_sobj;
}

class ViewRenderer {
  struct render_target {
    uint32_t depth_texture;
    uint32_t fbo;
  };

  std::unordered_map<uint32_t, render_target> m_renderTargetMap;

public:
  ViewRenderer() {}
  ~ViewRenderer() {}
  void render(uint32_t texc_id, uint32_t width, uint32_t height) {

    //  framebuffer
    auto found = m_renderTargetMap.find(texc_id);
    if (found == m_renderTargetMap.end()) {
      // Depth Buffer
      render_target rt;
      glGenRenderbuffers(1, &rt.depth_texture);
      glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_texture);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width,
                            height);

      // FBO
      glGenFramebuffers(1, &rt.fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texc_id, 0);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_RENDERBUFFER, rt.depth_texture);

      GLenum stat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      assert(stat == GL_FRAMEBUFFER_COMPLETE);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      vuloxr::Logger::Info(
          "SwapchainImage FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)", rt.fbo,
          texc_id, rt.depth_texture, width, height);

      found = m_renderTargetMap.insert({texc_id, rt}).first;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, found->second.fbo);

    glViewport(0, 0, width, height);

    glClearColor(1.0f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    //  Render
    auto sobj = &getShader();
    glUseProgram(sobj->program);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glEnableVertexAttribArray(sobj->loc_clr);
    glVertexAttribPointer(sobj->loc_vtx, 2, GL_FLOAT, GL_FALSE, 0, s_vtx);
    glVertexAttribPointer(sobj->loc_clr, 4, GL_FLOAT, GL_FALSE, 0, s_col);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
};

void xr_main_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
                  XrSystemId systemId, XrSession session, XrSpace appSpace,
                  std::span<const int64_t> formats,
                  //
                  const Graphics &graphics,
                  //
                  XrColor4f clearColor, XrEnvironmentBlendMode blendMode,
                  XrViewConfigurationType viewConfigurationType) {
  auto format = vuloxr::xr::selectColorSwapchainFormat(formats);

  // auto viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  // swapchain
  std::vector<std::shared_ptr<GLESSwapchain>> swapchains;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GLESSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
    swapchains.push_back(swapchain);
  }

  ViewRenderer renderer;

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

          renderer.render(image.image,
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

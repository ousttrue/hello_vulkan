#include "xr_session.h"
#include "render_scene.h"
#include <thread>
#include <vuloxr/xr/session.h>

using GLESSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLESKHR>;

struct viewsurface_t {
  uint32_t width;
  uint32_t height;
  std::shared_ptr<GLESSwapchain> swapchain;
  std::vector<render_target_t> rtarget_array;
};

/* ----------------------------------------------------------------------------
 * * Swapchain operation
 * ----------------------------------------------------------------------------
 * *
 *
 *  --+-- view[0] -- viewSurface[0]
 *    |
 *    +-- view[1] -- viewSurface[1]
 *                   +----------------------------------------------+
 *                   | uint32_t    width, height                    |
 *                   | XrSwapchain swapchain                        |
 *                   | rtarget_array[0]: (fbo_id, texc_id, texz_id) |
 *                   | rtarget_array[1]: (fbo_id, texc_id, texz_id) |
 *                   | rtarget_array[2]: (fbo_id, texc_id, texz_id) |
 *                   +----------------------------------------------+
 *
 * ----------------------------------------------------------------------------
 */
static int
oxr_alloc_swapchain_rtargets(GLESSwapchain &swapchain, uint32_t width,
                             uint32_t height,
                             std::vector<render_target_t> &rtarget_array) {

  for (uint32_t i = 0; i < swapchain.swapchainImages.size(); i++) {
    GLuint tex_c = swapchain.swapchainImages[i].image;
    GLuint tex_z = 0;
    GLuint fbo = 0;

    /* Depth Buffer */
    glGenRenderbuffers(1, &tex_z);
    glBindRenderbuffer(GL_RENDERBUFFER, tex_z);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    /* FBO */
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex_c, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, tex_z);

    GLenum stat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (stat != GL_FRAMEBUFFER_COMPLETE) {
      vuloxr::Logger::Error("FBO Imcomplete");
      return -1;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    render_target_t rtarget;
    rtarget.texc_id = tex_c;
    rtarget.texz_id = tex_z;
    rtarget.fbo_id = fbo;
    rtarget.width = width;
    rtarget.height = height;
    rtarget_array.push_back(rtarget);

    vuloxr::Logger::Info(
        "SwapchainImage[%d/%d] FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)", i,
        swapchain.swapchainImages.size(), fbo, tex_c, tex_z, width, height);
  }
  return 0;
}

static int oxr_release_viewsurface(viewsurface_t &viewSurface) {
  XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(viewSurface.swapchain->swapchain, &releaseInfo);

  return 0;
}

void xr_session(const std::function<bool(bool)> &runLoop, XrInstance instance,
                XrSystemId systemId, XrSession session, XrSpace appSpace) {

  init_gles_scene();

  //
  // setup view swapchain
  //
  auto viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);

  std::vector<viewsurface_t> m_viewSurface;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // auto &vp = conf[i];
    auto &vp = stereoscope.viewConfigurations[i];
    uint32_t vp_w = vp.recommendedImageRectWidth;
    uint32_t vp_h = vp.recommendedImageRectHeight;

    vuloxr::Logger::Info("Swapchain for view %d: WH(%d, %d), SampleCount=%d", i,
                         vp_w, vp_h, vp.recommendedSwapchainSampleCount);

    m_viewSurface.push_back({
        .width = vp_w,
        .height = vp_h,
        // sfc.swapchain = oxr_create_swapchain(session, sfc.width, sfc.height);
        .swapchain = std::make_shared<GLESSwapchain>(
            session, i, stereoscope.viewConfigurations[i], GL_RGBA8,
            XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR}),
    });

    auto &sfc = m_viewSurface.back();
    oxr_alloc_swapchain_rtargets(*sfc.swapchain, sfc.width, sfc.height,
                                 sfc.rtarget_array);
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

        /* Render each view */
        for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
          auto swapchain = m_viewSurface[i].swapchain;
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);

          {
            auto rtarget = m_viewSurface[i].rtarget_array[index];

            render_gles_scene(rtarget,
                              projectionLayer.subImage.imageRect.extent.width,
                              projectionLayer.subImage.imageRect.extent.height);

            oxr_release_viewsurface(m_viewSurface[i]);
          }

          composition.pushView(projectionLayer);
        }
      }
    }

    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers);

  } // while
}

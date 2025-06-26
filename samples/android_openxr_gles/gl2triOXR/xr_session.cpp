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

static XrEventDataBuffer s_evDataBuf;
static XrSessionState s_session_state = XR_SESSION_STATE_UNKNOWN;
static bool s_session_running = false;

bool oxr_is_session_running() { return s_session_running; }

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

static uint32_t oxr_acquire_swapchain_img(XrSwapchain swapchain) {
  uint32_t imgIdx;
  XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  xrAcquireSwapchainImage(swapchain, &acquireInfo, &imgIdx);

  XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  waitInfo.timeout = XR_INFINITE_DURATION;
  xrWaitSwapchainImage(swapchain, &waitInfo);

  return imgIdx;
}

static int oxr_acquire_viewsurface(viewsurface_t &viewSurface,
                                   render_target_t &rtarget,
                                   XrSwapchainSubImage &subImg) {
  subImg.swapchain = viewSurface.swapchain->swapchain;
  subImg.imageRect.offset.x = 0;
  subImg.imageRect.offset.y = 0;
  subImg.imageRect.extent.width = viewSurface.width;
  subImg.imageRect.extent.height = viewSurface.height;
  subImg.imageArrayIndex = 0;

  uint32_t imgIdx = oxr_acquire_swapchain_img(viewSurface.swapchain->swapchain);
  rtarget = viewSurface.rtarget_array[imgIdx];

  return 0;
}

static int oxr_locate_views(XrSession session, XrTime dpy_time, XrSpace space,
                            uint32_t *view_cnt, XrView *view_array) {
  XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

  XrViewState viewstat = {XR_TYPE_VIEW_STATE};
  uint32_t view_cnt_in = *view_cnt;
  uint32_t view_cnt_out;

  XrViewLocateInfo vloc = {XR_TYPE_VIEW_LOCATE_INFO};
  vloc.viewConfigurationType = viewType;
  vloc.displayTime = dpy_time;
  vloc.space = space;
  xrLocateViews(session, &vloc, &viewstat, view_cnt_in, &view_cnt_out,
                view_array);

  if ((viewstat.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
      (viewstat.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
    *view_cnt = 0; // There is no valid tracking poses for the views.
  }
  return 0;
}

static int oxr_release_viewsurface(viewsurface_t &viewSurface) {
  XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(viewSurface.swapchain->swapchain, &releaseInfo);

  return 0;
}

static int oxr_end_frame(XrSession session, XrTime dpy_time,
                         std::vector<XrCompositionLayerBaseHeader *> &layers) {
  XrFrameEndInfo frameEnd{XR_TYPE_FRAME_END_INFO};
  frameEnd.displayTime = dpy_time;
  frameEnd.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  frameEnd.layerCount = (uint32_t)layers.size();
  frameEnd.layers = layers.data();
  xrEndFrame(session, &frameEnd);

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

    /* Acquire View Location */
    uint32_t viewCount = (uint32_t)m_viewSurface.size();

    std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
    oxr_locate_views(session, frameState.predictedDisplayTime, appSpace,
                     &viewCount, views.data());

    /* Render each view */
    for (uint32_t i = 0; i < viewCount; i++) {
      XrSwapchainSubImage subImg;
      render_target_t rtarget;
      oxr_acquire_viewsurface(m_viewSurface[i], rtarget, subImg);

      render_gles_scene(rtarget, subImg.imageRect.extent.width,
                        subImg.imageRect.extent.height);

      oxr_release_viewsurface(m_viewSurface[i]);

      composition.pushView({
          .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
          .pose = views[i].pose,
          .fov = views[i].fov,
          .subImage = subImg,
      });
    }

    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers);
  }
}

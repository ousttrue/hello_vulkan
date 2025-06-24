#include <vuloxr/android/userdata.h>

#include <vuloxr/xr/platform/android.h>

#include <vuloxr/xr/graphics/egl.h>
#include <vuloxr/xr/session.h>

#include "render_scene.h"

auto APP_NAME = "hello_xr";

static EGLConfig _egl_get_config() {
  auto dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  auto ctx = eglGetCurrentContext();
  if (ctx == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  int cfg_id;
  eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &cfg_id);

  EGLint cfg_attribs[] = {EGL_CONFIG_ID, 0, EGL_NONE};
  cfg_attribs[1] = cfg_id;

  int ival;
  EGLConfig cfg;
  if (eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ival) != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  return cfg;
}

EGLContext _egl_get_context() {
  auto dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  auto ctx = eglGetCurrentContext();
  if (ctx == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  return ctx;
}

static XrViewConfigurationView *oxr_enumerate_viewconfig(XrInstance instance,
                                                         XrSystemId sysid,
                                                         uint32_t *numview) {
  uint32_t numConf;
  XrViewConfigurationView *conf;
  XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

  xrEnumerateViewConfigurationViews(instance, sysid, viewType, 0, &numConf,
                                    NULL);

  conf = (XrViewConfigurationView *)calloc(sizeof(XrViewConfigurationView),
                                           numConf);
  for (uint32_t i = 0; i < numConf; i++)
    conf[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;

  xrEnumerateViewConfigurationViews(instance, sysid, viewType, numConf,
                                    &numConf, conf);

  vuloxr::Logger::Info("ViewConfiguration num: %d", numConf);
  for (uint32_t i = 0; i < numConf; i++) {
    XrViewConfigurationView &vp = conf[i];
    vuloxr::Logger::Info(
        "ViewConfiguration[%d/%d]: MaxWH(%d, %d), MaxSample(%d)", i, numConf,
        vp.maxImageRectWidth, vp.maxImageRectHeight,
        vp.maxSwapchainSampleCount);
    vuloxr::Logger::Info("                        RecWH(%d, %d), RecSample(%d)",
                         vp.recommendedImageRectWidth,
                         vp.recommendedImageRectHeight,
                         vp.recommendedSwapchainSampleCount);
  }

  *numview = numConf;
  return conf;
}

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
static XrSwapchain oxr_create_swapchain(XrSession session, uint32_t width,
                                        uint32_t height) {
  XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  ci.usageFlags =
      XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  ci.format = GL_RGBA8;
  ci.width = width;
  ci.height = height;
  ci.faceCount = 1;
  ci.arraySize = 1;
  ci.mipCount = 1;
  ci.sampleCount = 1;

  XrSwapchain swapchain;
  xrCreateSwapchain(session, &ci, &swapchain);

  return swapchain;
}

static int
oxr_alloc_swapchain_rtargets(XrSwapchain swapchain, uint32_t width,
                             uint32_t height,
                             std::vector<render_target_t> &rtarget_array) {
  uint32_t imgCnt;
  xrEnumerateSwapchainImages(swapchain, 0, &imgCnt, NULL);

  XrSwapchainImageOpenGLESKHR *img_gles = (XrSwapchainImageOpenGLESKHR *)calloc(
      sizeof(XrSwapchainImageOpenGLESKHR), imgCnt);
  for (uint32_t i = 0; i < imgCnt; i++)
    img_gles[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;

  xrEnumerateSwapchainImages(swapchain, imgCnt, &imgCnt,
                             (XrSwapchainImageBaseHeader *)img_gles);

  for (uint32_t i = 0; i < imgCnt; i++) {
    GLuint tex_c = img_gles[i].image;
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
        "SwapchainImage[%d/%d] FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)", i, imgCnt,
        fbo, tex_c, tex_z, width, height);
  }
  free(img_gles);
  return 0;
}

struct viewsurface_t {
  uint32_t width;
  uint32_t height;
  XrSwapchain swapchain;
  std::vector<render_target_t> rtarget_array;
};

static std::vector<viewsurface_t> oxr_create_viewsurface(XrInstance instance,
                                                         XrSystemId sysid,
                                                         XrSession session) {
  std::vector<viewsurface_t> sfcArray;

  uint32_t viewCount;
  XrViewConfigurationView *conf_views =
      oxr_enumerate_viewconfig(instance, sysid, &viewCount);

  for (uint32_t i = 0; i < viewCount; i++) {
    const XrViewConfigurationView &vp = conf_views[i];
    uint32_t vp_w = vp.recommendedImageRectWidth;
    uint32_t vp_h = vp.recommendedImageRectHeight;

    vuloxr::Logger::Info("Swapchain for view %d: WH(%d, %d), SampleCount=%d", i,
                         vp_w, vp_h, vp.recommendedSwapchainSampleCount);

    viewsurface_t sfc;
    sfc.width = vp_w;
    sfc.height = vp_h;
    sfc.swapchain = oxr_create_swapchain(session, sfc.width, sfc.height);
    oxr_alloc_swapchain_rtargets(sfc.swapchain, sfc.width, sfc.height,
                                 sfc.rtarget_array);

    sfcArray.push_back(sfc);
  }

  return sfcArray;
}

static XrEventDataBuffer s_evDataBuf;

static XrEventDataBaseHeader *oxr_poll_event(XrInstance instance,
                                             XrSession session) {
  XrEventDataBaseHeader *ev =
      reinterpret_cast<XrEventDataBaseHeader *>(&s_evDataBuf);
  *ev = {XR_TYPE_EVENT_DATA_BUFFER};

  XrResult xr = xrPollEvent(instance, &s_evDataBuf);
  if (xr == XR_EVENT_UNAVAILABLE)
    return nullptr;

  if (xr != XR_SUCCESS) {
    vuloxr::Logger::Error("xrPollEvent");
    return NULL;
  }

  if (ev->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
    XrEventDataEventsLost *evLost =
        reinterpret_cast<XrEventDataEventsLost *>(ev);
    vuloxr::Logger::Warn("%p events lost", evLost);
  }
  return ev;
}

static XrSessionState s_session_state = XR_SESSION_STATE_UNKNOWN;
static bool s_session_running = false;

int oxr_begin_session(XrSession session) {
  XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

  XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
  bi.primaryViewConfigurationType = viewType;
  xrBeginSession(session, &bi);

  return 0;
}

int oxr_handle_session_state_changed(XrSession session,
                                     XrEventDataSessionStateChanged &ev,
                                     bool *exitLoop, bool *reqRestart) {
  XrSessionState old_state = s_session_state;
  XrSessionState new_state = ev.state;
  s_session_state = new_state;

  // LOGI("  [SessionState]: %s -> %s (session=%p, time=%ld)",
  //      to_string(old_state), to_string(new_state), ev.session, ev.time);

  if ((ev.session != XR_NULL_HANDLE) && (ev.session != session)) {
    vuloxr::Logger::Error("XrEventDataSessionStateChanged for unknown session");
    return -1;
  }

  switch (new_state) {
  case XR_SESSION_STATE_READY:
    oxr_begin_session(session);
    s_session_running = true;
    break;

  case XR_SESSION_STATE_STOPPING:
    xrEndSession(session);
    s_session_running = false;
    break;

  case XR_SESSION_STATE_EXITING:
    *exitLoop = true;
    *reqRestart =
        false; // Do not attempt to restart because user closed this session.
    break;

  case XR_SESSION_STATE_LOSS_PENDING:
    *exitLoop = true;
    *reqRestart = true; // Poll for a new instance.
    break;

  default:
    break;
  }
  return 0;
}

int oxr_poll_events(XrInstance instance, XrSession session, bool *exit_loop,
                    bool *req_restart) {
  *exit_loop = false;
  *req_restart = false;

  // Process all pending messages.
  while (XrEventDataBaseHeader *ev = oxr_poll_event(instance, session)) {
    switch (ev->type) {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
      vuloxr::Logger::Warn("XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING");
      *exit_loop = true;
      *req_restart = true;
      return -1;
    }

    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      vuloxr::Logger::Warn("XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED");
      XrEventDataSessionStateChanged sess_ev =
          *(XrEventDataSessionStateChanged *)ev;
      oxr_handle_session_state_changed(session, sess_ev, exit_loop,
                                       req_restart);
      break;
    }

    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      vuloxr::Logger::Warn("XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED");
      break;

    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      vuloxr::Logger::Warn("XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING");
      break;

    default:
      vuloxr::Logger::Error("Unknown event type %d", ev->type);
      break;
    }
  }
  return 0;
}

bool oxr_is_session_running() { return s_session_running; }

int oxr_begin_frame(XrSession session, XrTime *dpy_time) {
  XrFrameWaitInfo frameWait = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState = {XR_TYPE_FRAME_STATE};
  xrWaitFrame(session, &frameWait, &frameState);

  XrFrameBeginInfo frameBegin = {XR_TYPE_FRAME_BEGIN_INFO};
  xrBeginFrame(session, &frameBegin);

  *dpy_time = frameState.predictedDisplayTime;
  return (int)frameState.shouldRender;
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

static uint32_t oxr_acquire_swapchain_img(XrSwapchain swapchain) {
  uint32_t imgIdx;
  XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  xrAcquireSwapchainImage(swapchain, &acquireInfo, &imgIdx);

  XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  waitInfo.timeout = XR_INFINITE_DURATION;
  xrWaitSwapchainImage(swapchain, &waitInfo);

  return imgIdx;
}

int oxr_acquire_viewsurface(viewsurface_t &viewSurface,
                            render_target_t &rtarget,
                            XrSwapchainSubImage &subImg) {
  subImg.swapchain = viewSurface.swapchain;
  subImg.imageRect.offset.x = 0;
  subImg.imageRect.offset.y = 0;
  subImg.imageRect.extent.width = viewSurface.width;
  subImg.imageRect.extent.height = viewSurface.height;
  subImg.imageArrayIndex = 0;

  uint32_t imgIdx = oxr_acquire_swapchain_img(viewSurface.swapchain);
  rtarget = viewSurface.rtarget_array[imgIdx];

  return 0;
}

int oxr_release_viewsurface(viewsurface_t &viewSurface) {
  XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(viewSurface.swapchain, &releaseInfo);

  return 0;
}

int oxr_end_frame(XrSession session, XrTime dpy_time,
                  std::vector<XrCompositionLayerBaseHeader *> &layers) {
  XrFrameEndInfo frameEnd{XR_TYPE_FRAME_END_INFO};
  frameEnd.displayTime = dpy_time;
  frameEnd.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  frameEnd.layerCount = (uint32_t)layers.size();
  frameEnd.layers = layers.data();
  xrEndFrame(session, &frameEnd);

  return 0;
}

static void xr_gles_session(const std::function<bool(bool)> &runLoop,
                            XrInstance instance, XrSystemId systemId,
                            XrSession session, XrSpace appSpace) {

  init_gles_scene();

  auto m_viewSurface = oxr_create_viewsurface(instance, systemId, session);

  while (runLoop(true)) {
    bool exit_loop, req_restart;
    oxr_poll_events(instance, session, &exit_loop, &req_restart);

    if (!oxr_is_session_running()) {
      continue;
    }

    std::vector<XrCompositionLayerBaseHeader *> all_layers;

    XrTime dpy_time;
    oxr_begin_frame(session, &dpy_time);

    std::vector<XrCompositionLayerProjectionView> layerViews;

    /* Acquire View Location */
    uint32_t viewCount = (uint32_t)m_viewSurface.size();

    std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
    oxr_locate_views(session, dpy_time, appSpace, &viewCount, views.data());

    layerViews.resize(viewCount);

    /* Render each view */
    for (uint32_t i = 0; i < viewCount; i++) {
      XrSwapchainSubImage subImg;
      render_target_t rtarget;
      oxr_acquire_viewsurface(m_viewSurface[i], rtarget, subImg);

      render_gles_scene(rtarget, subImg.imageRect.extent.width,
                        subImg.imageRect.extent.height);

      oxr_release_viewsurface(m_viewSurface[i]);

      layerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
      layerViews[i].pose = views[i].pose;
      layerViews[i].fov = views[i].fov;
      layerViews[i].subImage = subImg;
    }

    XrCompositionLayerProjection layer{
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        .space = appSpace,
        .viewCount = (uint32_t)layerViews.size(),
        .views = layerViews.data(),
    };

    all_layers.push_back(
        reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));

    /* Compose all layers */
    oxr_end_frame(session, dpy_time, all_layers);
  }
}

void android_main(struct android_app *app) {
#ifdef NDEBUG
  vuloxr::Logger::Info("#### [release][android_main] ####");
#else
  vuloxr::Logger::Info("#### [debug][android_main] ####");
#endif

  vuloxr::android::UserData userdata{
      .pApp = app,
      ._appName = APP_NAME,
  };
  app->userData = &userdata;
  app->onAppCmd = vuloxr::android::UserData::on_app_cmd;
  app->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  JNIEnv *Env;
  app->activity->vm->AttachCurrentThread(&Env, nullptr);

  vuloxr::xr::Instance xr_instance;
  xr_instance.extensions.push_back(
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
  xr_instance.extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);

  auto instanceCreateInfoAndroid = vuloxr::xr::androidLoader(app);
  vuloxr::xr::CheckXrResult(xr_instance.create(&instanceCreateInfoAndroid));

  vuloxr::xr::createEgl(xr_instance.instance, xr_instance.systemId);

  vuloxr::xr::getGLESGraphicsRequirementsKHR(xr_instance.instance,
                                             xr_instance.systemId);
  {
    {
      XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {
          .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
          .display = eglGetCurrentDisplay(),
          .config = _egl_get_config(),
          .context = _egl_get_context(),
      };
      vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
                                  &graphicsBinding);

      XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
          .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
          .next = 0,
          .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
          .poseInReferenceSpace = {.orientation = {0, 0, 0, 1.0f},
                                   .position = {0, 0, 0}},
      };
      XrSpace appSpace;
      vuloxr::xr::CheckXrResult(xrCreateReferenceSpace(
          session, &referenceSpaceCreateInfo, &appSpace));

      xr_gles_session(
          [app](bool isSessionRunning) {
            if (app->destroyRequested) {
              return false;
            }

            // Read all pending events.
            for (;;) {
              int events;
              struct android_poll_source *source;
              // If the timeout is zero, returns immediately without blocking.
              // If the timeout is negative, waits indefinitely until an event
              // appears.
              const int timeoutMilliseconds =
                  (!((vuloxr::android::UserData *)app->userData)->_active &&
                   !isSessionRunning && app->destroyRequested == 0)
                      ? -1
                      : 0;
              if (ALooper_pollOnce(timeoutMilliseconds, nullptr, &events,
                                   (void **)&source) < 0) {
                break;
              }

              // Process this event.
              if (source != nullptr) {
                source->process(app, source);
              }
            }

            return true;
          },
          xr_instance.instance, xr_instance.systemId, session, appSpace);
      // session scope
    }
    // vulkan scope
  }

  ANativeActivity_finish(app->activity);

  // Read all pending events.
  for (;;) {
    int events;
    struct android_poll_source *source;
    if (ALooper_pollOnce(-1, nullptr, &events, (void **)&source) < 0) {
      break;
    }

    // Process this event.
    if (source != nullptr) {
      source->process(app, source);
    }
  }

  app->activity->vm->DetachCurrentThread();
}

#include "app_engine.h"

#include <EGL/egl.h>

#include <EGL/eglext.h>

#include <vuloxr/android/userdata.h>

#include <cstddef>
#include <vuloxr/android/xr_loader.h>
#include <vuloxr/xr/session.h>
#include <vuloxr/xr/egl.h>

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

void xr_gles_session(const std::function<bool(bool)> &runLoop,
                     AppEngine &engine) {

  while (runLoop(true)) {
    engine.UpdateFrame();
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

  AppEngine engine(app, xr_instance.instance, xr_instance.systemId);

  {

    {
      // XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {
      //     .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
      //     .display = eglGetCurrentDisplay(),
      //     .config = _egl_get_config(),
      //     .context = _egl_get_context(),
      // };
      // vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
      //                             &graphicsBinding);

      // XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
      //     .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      //     .next = 0,
      //     .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
      //     .poseInReferenceSpace = {.orientation = {0, 0, 0, 1.0f},
      //                              .position = {0, 0, 0}},
      // };
      // XrSpace appSpace;
      // vuloxr::xr::CheckXrResult(xrCreateReferenceSpace(
      //     session, &referenceSpaceCreateInfo, &appSpace));

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
          engine);
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

#include <vuloxr/xr/platform/android.h>

#include <vuloxr/android/userdata.h>

#include "xr_main_loop.h"
#include <cstddef>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vuloxr/vk/swapchain.h>
#include <vuloxr/xr/graphics/vulkan.h>
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <vuloxr/xr/graphics/egl.h>
#endif

#include <vuloxr/xr/session.h>

auto APP_NAME = "hello_xr";

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
#ifdef XR_USE_GRAPHICS_API_VULKAN
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
#define createGraphics vuloxr::xr::createVulkan
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
  xr_instance.extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
#endif

  auto instanceCreateInfoAndroid = vuloxr::xr::androidLoader(app);
  vuloxr::xr::CheckXrResult(xr_instance.create(&instanceCreateInfoAndroid));

  {
    auto [graphics, graphicsBinding] =
        createGraphics(xr_instance.instance, xr_instance.systemId);

    {
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

      auto runLoop = [app](bool isSessionRunning) {
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
      };

      xr_main_loop(runLoop, xr_instance.instance, xr_instance.systemId, session,
                   appSpace, session.formats,
                   //
                   graphics, {0.184313729f, 0.309803933f, 0.309803933f, 1.0f});
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

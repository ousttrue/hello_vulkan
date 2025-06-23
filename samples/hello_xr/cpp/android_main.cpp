#include <vuloxr/android/userdata.h>

#include "GetXrReferenceSpaceCreateInfo.h"
#include "xr_vulkan_session.h"
#include <cstddef>
#include <vuloxr/xr/session.h>

auto APP_NAME = "hello_xr";

void _android_main(struct android_app *app) {
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

  // Initialize the loader for this platform
  PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
  if (XR_SUCCEEDED(
          xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                (PFN_xrVoidFunction *)(&initializeLoader)))) {
    XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid = {
        .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
        .applicationVM = app->activity->vm,
        .applicationContext = app->activity->clazz,
    };
    initializeLoader(
        (const XrLoaderInitInfoBaseHeaderKHR *)&loaderInitInfoAndroid);
  }

  XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid{
      .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
      .applicationVM = app->activity->vm,
      .applicationActivity = app->activity->clazz,
  };
  vuloxr::xr::Instance xr_instance;
  xr_instance.extensions.push_back(
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
  vuloxr::xr::CheckXrResult(xr_instance.create(&instanceCreateInfoAndroid));

  {
    auto [instance, physicalDevice, device] = xr_instance.createVulkan();

    {
      vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
                                  instance, physicalDevice,
                                  physicalDevice.graphicsFamilyIndex, device);

      auto referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo();
      XrSpace appSpace;
      vuloxr::xr::CheckXrResult(xrCreateReferenceSpace(
          session, &referenceSpaceCreateInfo, &appSpace));
      //       auto clearColor = options.GetBackgroundClearColor();
      // VkClearColorValue Options::GetBackgroundClearColor() const {
      //   static const VkClearColorValue SlateGrey{
      //       .float32 = {0.184313729f, 0.309803933f, 0.309803933f, 1.0f}};
      //   static const VkClearColorValue TransparentBlack{
      //       .float32 = {0.0f, 0.0f, 0.0f, 0.0f}};
      //   static const VkClearColorValue Black{.float32 = {0.0f, 0.0f,
      //   0.0f, 1.0f}};
      //
      //   switch (Parsed.EnvironmentBlendMode) {
      //   case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
      //     return SlateGrey;
      //   case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
      //     return Black;
      //   case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
      //     return TransparentBlack;
      //   default:
      //     return SlateGrey;
      //   }
      // }

      xr_vulkan_session(
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
          xr_instance.instance, xr_instance.systemId, session, appSpace,
          //
          session.selectColorSwapchainFormat(), physicalDevice,
          physicalDevice.graphicsFamilyIndex, device,
          //
          {{0.184313729f, 0.309803933f, 0.309803933f, 1.0f}});
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

void android_main(struct android_app *app) {
  try {
    _android_main(app);
  } catch (const std::exception &ex) {
    vuloxr::Logger::Error("%s", ex.what());
  } catch (...) {
    vuloxr::Logger::Error("Unknown Error");
  }
}

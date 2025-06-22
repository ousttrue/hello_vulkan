#include "GetXrReferenceSpaceCreateInfo.h"
#include "options.h"
#include "xr_vulkan_session.h"
#include <cstddef>
#include <vuloxr/android_userdata.h>
#include <vuloxr/xr.h>
#include <xro/xro.h>

auto APP_NAME = "hello_xr";

static void ShowHelp() {
  vuloxr::Logger::Info("adb shell setprop debug.xr.formFactor Hmd|Handheld");
  vuloxr::Logger::Info(
      "adb shell setprop debug.xr.viewConfiguration Stereo|Mono");
  vuloxr::Logger::Info(
      "adb shell setprop debug.xr.blendMode Opaque|Additive|AlphaBlend");
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
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

  Options options;
  if (!options.UpdateOptionsFromSystemProperties()) {
    ShowHelp();
    return;
  }

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
  xr_instance.systemInfo.formFactor = options.Parsed.FormFactor;
  vuloxr::xr::CheckXrResult(xr_instance.create(&instanceCreateInfoAndroid));
  // options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  // if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
  //   ShowHelp();
  // }

  {
    auto [instance, physicalDevice, device] = xr_instance.createVulkan();

    // debug
    vko::g_vkSetDebugUtilsObjectNameEXT(instance);

    {
      xro::Session session(xr_instance.instance, xr_instance.systemId, instance,
                           physicalDevice, physicalDevice.graphicsFamilyIndex,
                           device);

      XrReferenceSpaceCreateInfo referenceSpaceCreateInfo =
          GetXrReferenceSpaceCreateInfo(options.AppSpace);
      XrSpace appSpace;
      XRO_CHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo,
                                       &appSpace));
      auto clearColor = options.GetBackgroundClearColor();
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
          options.Parsed.EnvironmentBlendMode, clearColor,
          options.Parsed.ViewConfigType, session.selectColorSwapchainFormat(),
          physicalDevice, physicalDevice.graphicsFamilyIndex, device);

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

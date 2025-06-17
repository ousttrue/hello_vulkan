#include "GetXrReferenceSpaceCreateInfo.h"
#include "VulkanDebugMessageThunk.h"
#include "options.h"
#include "xr_loop.h"
#include <vko/android_userdata.h>
#include <xro/xro.h>

auto APP_NAME = "hello_xr";

static void ShowHelp() {
  xro::Logger::Info("adb shell setprop debug.xr.formFactor Hmd|Handheld");
  xro::Logger::Info("adb shell setprop debug.xr.viewConfiguration Stereo|Mono");
  xro::Logger::Info(
      "adb shell setprop debug.xr.blendMode Opaque|Additive|AlphaBlend");
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void _android_main(struct android_app *app) {
#ifdef NDEBUG
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [release][android_main] ####");
#else
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [debug][android_main] ####");
#endif

  vko::UserData userdata{
      .pApp = app,
      ._appName = APP_NAME,
  };
  app->userData = &userdata;
  app->onAppCmd = vko::UserData::on_app_cmd;
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
  xro::Instance xr_instance;
  xr_instance.extensions.push_back(
      XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
  xr_instance.systemInfo.formFactor = options.Parsed.FormFactor;
  XRO_CHECK(xr_instance.create(&instanceCreateInfoAndroid));
  // options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  // if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
  //   ShowHelp();
  // }

  {
    auto [instance, physicalDevice, device] =
        xr_instance.createVulkan(debugMessageThunk);

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
      xr_loop(
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
                  (!((vko::UserData *)app->userData)->_active &&
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
    xro::Logger::Error("%s", ex.what());
  } catch (...) {
    xro::Logger::Error("Unknown Error");
  }
}

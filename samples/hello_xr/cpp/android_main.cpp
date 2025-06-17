#include <vulkan/vulkan.h>

#include <android_native_app_glue.h>

#include <openxr/openxr_platform.h>

#include "openxr_program/VulkanDebugMessageThunk.h"
#include "openxr_program/openxr_program.h"
#include "openxr_program/options.h"

#include <common/logger.h>

#include <vko/android_userdata.h>
#include <vko/vko.h>

#include "xr_loop.h"
#include <thread>
#include <xro/xro.h>

auto APP_NAME = "hello_xr";

static void ShowHelp() {
  Log::Write(Log::Level::Info,
             "adb shell setprop debug.xr.formFactor Hmd|Handheld");
  Log::Write(Log::Level::Info,
             "adb shell setprop debug.xr.viewConfiguration Stereo|Mono");
  Log::Write(Log::Level::Info,
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

  auto [instance, physicalDevice, device] =
      xr_instance.createVulkan(debugMessageThunk);

  // XrSession
  auto session = OpenXrSession::create(
      options, xr_instance.instance, xr_instance.systemId, instance,
      physicalDevice, physicalDevice.graphicsFamilyIndex, device);

  xr_loop(
      [app, &session]() {
        while (true) {
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
                 !session.m_state.IsSessionRunning() &&
                 app->destroyRequested == 0)
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

          auto poll = session.m_state.PollEvents();
          if (poll.exitRenderLoop) {
            ANativeActivity_finish(app->activity);
            continue;
          }

          if (!session.m_state.IsSessionRunning()) {
            // Throttle loop since xrWaitFrame won't be called.
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
          }

          session.m_input.PollActions(session.m_session);

          return true;
        }
      },
      options, session, physicalDevice, physicalDevice.graphicsFamilyIndex,
      device);

  app->activity->vm->DetachCurrentThread();
}

void android_main(struct android_app *app) {
  try {
    _android_main(app);
  } catch (const std::exception &ex) {
    Log::Write(Log::Level::Error, ex.what());
  } catch (...) {
    Log::Write(Log::Level::Error, "Unknown Error");
  }
}

#include "VulkanGraphicsPlugin.h"
#include "logger.h"
#include "openxr_program.h"
#include "options.h"
#include "platformplugin_android.h"
#include <android_native_app_glue.h>
#include <sys/system_properties.h>

#include <thread>

struct AndroidAppState {
  ANativeWindow *NativeWindow = nullptr;
  bool Resumed = false;
};

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app *app, int32_t cmd) {
  AndroidAppState *appState = (AndroidAppState *)app->userData;

  switch (cmd) {
  // There is no APP_CMD_CREATE. The ANativeActivity creates the
  // application thread from onCreate(). The application thread
  // then calls android_main().
  case APP_CMD_START: {
    Log::Write(Log::Level::Info, "    APP_CMD_START");
    Log::Write(Log::Level::Info, "onStart()");
    break;
  }
  case APP_CMD_RESUME: {
    Log::Write(Log::Level::Info, "onResume()");
    Log::Write(Log::Level::Info, "    APP_CMD_RESUME");
    appState->Resumed = true;
    break;
  }
  case APP_CMD_PAUSE: {
    Log::Write(Log::Level::Info, "onPause()");
    Log::Write(Log::Level::Info, "    APP_CMD_PAUSE");
    appState->Resumed = false;
    break;
  }
  case APP_CMD_STOP: {
    Log::Write(Log::Level::Info, "onStop()");
    Log::Write(Log::Level::Info, "    APP_CMD_STOP");
    break;
  }
  case APP_CMD_DESTROY: {
    Log::Write(Log::Level::Info, "onDestroy()");
    Log::Write(Log::Level::Info, "    APP_CMD_DESTROY");
    appState->NativeWindow = NULL;
    break;
  }
  case APP_CMD_INIT_WINDOW: {
    Log::Write(Log::Level::Info, "surfaceCreated()");
    Log::Write(Log::Level::Info, "    APP_CMD_INIT_WINDOW");
    appState->NativeWindow = app->window;
    break;
  }
  case APP_CMD_TERM_WINDOW: {
    Log::Write(Log::Level::Info, "surfaceDestroyed()");
    Log::Write(Log::Level::Info, "    APP_CMD_TERM_WINDOW");
    appState->NativeWindow = NULL;
    break;
  }
  }
}

void ShowHelp() {
  Log::Write(Log::Level::Info,
             "adb shell setprop debug.xr.formFactor Hmd|Handheld");
  Log::Write(Log::Level::Info,
             "adb shell setprop debug.xr.viewConfiguration Stereo|Mono");
  Log::Write(Log::Level::Info,
             "adb shell setprop debug.xr.blendMode Opaque|Additive|AlphaBlend");
}

bool UpdateOptionsFromSystemProperties(Options &options) {
  char value[PROP_VALUE_MAX] = {};

  if (__system_property_get("debug.xr.formFactor", value) != 0) {
    options.FormFactor = value;
  }

  if (__system_property_get("debug.xr.viewConfiguration", value) != 0) {
    options.ViewConfiguration = value;
  }

  if (__system_property_get("debug.xr.blendMode", value) != 0) {
    options.EnvironmentBlendMode = value;
  }

  try {
    options.ParseStrings();
  } catch (std::invalid_argument &ia) {
    Log::Write(Log::Level::Error, ia.what());
    ShowHelp();
    return false;
  }
  return true;
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app *app) {
  try {
    JNIEnv *Env;
    app->activity->vm->AttachCurrentThread(&Env, nullptr);

    AndroidAppState appState = {};

    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    std::shared_ptr<Options> options = std::make_shared<Options>();
    if (!UpdateOptionsFromSystemProperties(*options)) {
      return;
    }

    std::shared_ptr<PlatformData> data = std::make_shared<PlatformData>();
    data->applicationVM = app->activity->vm;
    data->applicationActivity = app->activity->clazz;

    bool requestRestart = false;
    bool exitRenderLoop = false;

    // Create platform-specific implementation.
    std::shared_ptr<IPlatformPlugin> platformPlugin =
        CreatePlatformPlugin_Android(options, data);

    // Create graphics API implementation.
    std::shared_ptr<VulkanGraphicsPlugin> graphicsPlugin =
        std::make_shared<VulkanGraphicsPlugin>(options, platformPlugin);

    // Initialize the OpenXR program.
    auto program = std::make_shared<OpenXrProgram>(options, platformPlugin,
                                                   graphicsPlugin);

    // Initialize the loader for this platform
    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (XR_SUCCEEDED(
            xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                  (PFN_xrVoidFunction *)(&initializeLoader)))) {
      XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid = {
          XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
      loaderInitInfoAndroid.applicationVM = app->activity->vm;
      loaderInitInfoAndroid.applicationContext = app->activity->clazz;
      initializeLoader(
          (const XrLoaderInitInfoBaseHeaderKHR *)&loaderInitInfoAndroid);
    }

    program->CreateInstance();
    program->InitializeSystem();

    options->SetEnvironmentBlendMode(program->GetPreferredBlendMode());
    UpdateOptionsFromSystemProperties(*options);
    platformPlugin->UpdateOptions(options);
    graphicsPlugin->UpdateOptions(options);

    program->InitializeDevice();
    program->InitializeSession();
    program->CreateSwapchains();

    while (app->destroyRequested == 0) {
      // Read all pending events.
      for (;;) {
        int events;
        struct android_poll_source *source;
        // If the timeout is zero, returns immediately without blocking.
        // If the timeout is negative, waits indefinitely until an event
        // appears.
        const int timeoutMilliseconds =
            (!appState.Resumed && !program->IsSessionRunning() &&
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

      program->PollEvents(&exitRenderLoop, &requestRestart);
      if (exitRenderLoop) {
        ANativeActivity_finish(app->activity);
        continue;
      }

      if (!program->IsSessionRunning()) {
        // Throttle loop since xrWaitFrame won't be called.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        continue;
      }

      program->PollActions();
      program->RenderFrame();
    }

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &ex) {
    Log::Write(Log::Level::Error, ex.what());
  } catch (...) {
    Log::Write(Log::Level::Error, "Unknown Error");
  }
}

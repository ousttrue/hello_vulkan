#include <vulkan/vulkan.h>

#include <android_native_app_glue.h>

#include <openxr/openxr_platform.h>

#include "CubeScene.h"
#include "VulkanGraphicsPlugin.h"
#include "logger.h"
#include "openxr_program.h"
#include "options.h"
#include "vulkan_layers.h"

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

    Options options;
    if (!options.UpdateOptionsFromSystemProperties()) {
      ShowHelp();
      return;
    }

    bool requestRestart = false;
    bool exitRenderLoop = false;

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

    XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid{
        .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
        .applicationVM = app->activity->vm,
        .applicationActivity = app->activity->clazz,
    };

    // Initialize the OpenXR program.
    auto program = OpenXrProgram::Create(
        options, {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME},
        &instanceCreateInfoAndroid);

    options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
    if (!options.UpdateOptionsFromSystemProperties()) {
      ShowHelp();
    }

    auto vulkan = program->InitializeDevice(getVulkanLayers(),
                                            getVulkanInstanceExtensions(),
                                            getVulkanDeviceExtensions());
    program->InitializeSession(vulkan);
    auto projectionLayer = program->CreateSwapchains(vulkan);

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

      std::vector<XrCompositionLayerBaseHeader *> layers;
      auto frameState = program->BeginFrame();
      if (frameState.shouldRender == XR_TRUE) {
        if (projectionLayer->UpdateLocateView(program->m_session,
                                              program->m_appSpace,
                                              frameState.predictedDisplayTime,
                                              options.Parsed.ViewConfigType)) {

          CubeScene scene;
          scene.addSpaceCubes(program->m_appSpace,
                              frameState.predictedDisplayTime,
                              program->m_visualizedSpaces);
          scene.addHandCubes(program->m_appSpace,
                             frameState.predictedDisplayTime, program->m_input);

          if (auto layer = projectionLayer->RenderLayer(
                  program->m_appSpace, scene.cubes, vulkan,
                  options.GetBackgroundClearColor(),
                  options.Parsed.EnvironmentBlendMode)) {
            layers.push_back(
                reinterpret_cast<XrCompositionLayerBaseHeader *>(layer));
          }
        }
      }
      program->EndFrame(frameState.predictedDisplayPeriod, layers);
    }

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &ex) {
    Log::Write(Log::Level::Error, ex.what());
  } catch (...) {
    Log::Write(Log::Level::Error, "Unknown Error");
  }
}

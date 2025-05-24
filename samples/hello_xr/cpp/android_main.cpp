#include <vulkan/vulkan.h>

#include <android_native_app_glue.h>

#include <openxr/openxr_platform.h>

#include "Swapchain.h"
#include "logger.h"
#include "openxr_program.h"
#include "openxr_session.h"
#include "options.h"
#include "vkr/CubeScene.h"
#include "vkr/VulkanRenderer.h"
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

    auto vulkan = program->InitializeVulkan(getVulkanLayers(),
                                            getVulkanInstanceExtensions(),
                                            getVulkanDeviceExtensions());

    // XrSession
    auto session = program->InitializeSession(vulkan);

    // Create resources for each view.
    auto config = session->GetSwapchainConfiguration();
    std::vector<std::shared_ptr<Swapchain>> swapchains;
    std::vector<std::shared_ptr<VulkanRenderer>> renderers;
    for (uint32_t i = 0; i < config.Views.size(); i++) {
      // XrSwapchain
      auto swapchain = Swapchain::Create(session->m_session, i, config.Views[i],
                                         config.Format);
      swapchains.push_back(swapchain);

      // VkPipeline... etc
      auto renderer = std::make_shared<VulkanRenderer>(
          vulkan.PhysicalDevice, vulkan.Device, vulkan.QueueFamilyIndex,
          VkExtent2D{swapchain->m_swapchainCreateInfo.width,
                     swapchain->m_swapchainCreateInfo.height},
          static_cast<VkFormat>(swapchain->m_swapchainCreateInfo.format),
          static_cast<VkSampleCountFlagBits>(
              swapchain->m_swapchainCreateInfo.sampleCount));
      renderers.push_back(renderer);
    }

    // mainloop
    while (app->destroyRequested == 0) {
      // Read all pending events.
      for (;;) {
        int events;
        struct android_poll_source *source;
        // If the timeout is zero, returns immediately without blocking.
        // If the timeout is negative, waits indefinitely until an event
        // appears.
        const int timeoutMilliseconds =
            (!appState.Resumed && !session->IsSessionRunning() &&
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

      session->PollEvents(&exitRenderLoop, &requestRestart);
      if (exitRenderLoop) {
        ANativeActivity_finish(app->activity);
        continue;
      }

      if (!session->IsSessionRunning()) {
        // Throttle loop since xrWaitFrame won't be called.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        continue;
      }

      session->PollActions();

      auto frameState = session->BeginFrame();
      LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                   session->m_appSpace);

      if (frameState.shouldRender == XR_TRUE) {
        uint32_t viewCountOutput;
        if (session->LocateView(
                session->m_appSpace, frameState.predictedDisplayTime,
                options.Parsed.ViewConfigType, &viewCountOutput)) {
          // XrCompositionLayerProjection

          // update scene
          CubeScene scene;
          scene.addSpaceCubes(session->m_appSpace,
                              frameState.predictedDisplayTime,
                              session->m_visualizedSpaces);
          scene.addHandCubes(session->m_appSpace,
                             frameState.predictedDisplayTime, session->m_input);

          for (uint32_t i = 0; i < viewCountOutput; ++i) {
            // XrCompositionLayerProjectionView(left / right)
            auto swapchain = swapchains[i];
            auto info = swapchain->AcquireSwapchain(session->m_views[i]);
            composition.pushView(info.CompositionLayer);

            {
              // render vulkan
              auto renderer = renderers[i];
              VkCommandBuffer cmd = renderer->BeginCommand();
              renderer->RenderView(
                  cmd, info.Image, options.GetBackgroundClearColor(),
                  scene.CalcCubeMatrices(info.calcViewProjection()));
              renderer->EndCommand(cmd);
            }

            swapchain->EndSwapchain();
          }
        }
      }

      // std::vector<XrCompositionLayerBaseHeader *>
      auto &layers = composition.commitLayers();
      session->EndFrame(frameState.predictedDisplayPeriod, layers);
    }

    app->activity->vm->DetachCurrentThread();
  } catch (const std::exception &ex) {
    Log::Write(Log::Level::Error, ex.what());
  } catch (...) {
    Log::Write(Log::Level::Error, "Unknown Error");
  }
}

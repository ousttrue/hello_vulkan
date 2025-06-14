#include <vulkan/vulkan.h>

#include <android_native_app_glue.h>

#include <openxr/openxr_platform.h>

#include "openxr_program/VulkanDebugMessageThunk.h"
#include "openxr_program/openxr_program.h"
#include "openxr_program/options.h"

#include <common/logger.h>
#include <vkr/vulkan_renderer.h>

#include <vko/android_userdata.h>
#include <vko/vko.h>

#include "xr_loop.h"
#include <thread>

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
void android_main(struct android_app *app) {
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

  vko::Instance instance;
  instance.appInfo.pApplicationName = "hello_xr";
  instance.appInfo.pEngineName = "hello_xr";
  instance.debugUtilsMessengerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugMessageThunk,
      .pUserData = program.get(),
  };
  vko::Device device;

#ifdef NDEBUG
#else
  instance.debugUtilsMessengerCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  instance.pushFirstSupportedValidationLayer({
      "VK_LAYER_KHRONOS_validation",
      "VK_LAYER_LUNARG_standard_validation",
  });
  instance.pushFirstSupportedInstanceExtension({
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
  });
#endif

  auto vulkan = program->InitializeVulkan(
      instance.validationLayers, instance.instanceExtensions,
      device.deviceExtensions, &instance.debugUtilsMessengerCreateInfo);
  instance.reset(vulkan.Instance);
  device.reset(vulkan.Device);

  // XrSession
  auto session = program->InitializeSession(vulkan);

  xr_loop(
      [app, session]() {
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
                 !session->IsSessionRunning() && app->destroyRequested == 0)
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

          bool requestRestart = false;
          bool exitRenderLoop = false;
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

          return true;
        }
      },
      options, session, vulkan.PhysicalDevice, vulkan.QueueFamilyIndex,
      vulkan.Device);

  app->activity->vm->DetachCurrentThread();
}

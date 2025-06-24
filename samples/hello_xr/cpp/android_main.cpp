#include <vuloxr/android/userdata.h>

#include "xr_vulkan_session.h"
#include <cstddef>
#include <vuloxr/android/xr_loader.h>
#include <vuloxr/xr/session.h>
#include <vuloxr/xr/vulkan.h>

auto APP_NAME = "hello_xr";

// List of supported color swapchain formats.
constexpr VkFormat SupportedColorSwapchainFormats[] = {
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_B8G8R8A8_UNORM,
    VK_FORMAT_R8G8B8A8_UNORM,
};

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
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);

  auto instanceCreateInfoAndroid = vuloxr::xr::androidLoader(app);
  vuloxr::xr::CheckXrResult(xr_instance.create(&instanceCreateInfoAndroid));

  {
    auto [instance, physicalDevice, device] =
        vuloxr::xr::createVulkan(xr_instance.instance, xr_instance.systemId);

    {
      XrGraphicsBindingVulkan2KHR graphicsBinding{
          .type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
          .next = nullptr,
          .instance = instance,
          .physicalDevice = physicalDevice,
          .device = device,
          .queueFamilyIndex = physicalDevice.graphicsFamilyIndex,
          .queueIndex = 0,
      };

      vuloxr::xr::Session session(xr_instance.instance, xr_instance.systemId,
                                  &graphicsBinding);
      auto selectedFormat = vuloxr::vk::selectColorSwapchainFormat(
          session.formats, SupportedColorSwapchainFormats);
      assert(selectedFormat);

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
          *selectedFormat, physicalDevice, physicalDevice.graphicsFamilyIndex,
          device,
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

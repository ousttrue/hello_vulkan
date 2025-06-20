#include <assert.h>
#include <vuloxr/android_userdata.h>

#include "main_loop.h"
#include "vuloxr/vk.h"

auto APP_NAME = "vuloxr";

static bool _main_loop(android_app *app, vuloxr::android::UserData *userdata) {
  vuloxr::Logger::Info("## main_loop");
  vuloxr::vk::Instance instance;
  // instance.appInfo.pApplicationName = "vulfwk";
  // instance.appInfo.pEngineName = "vulfwk";
  instance.extensions = {"VK_KHR_surface", "VK_KHR_android_surface"};
#ifdef NDEBUG
#else
  instance.addLayer("VK_LAYER_KHRONOS_validation");
  instance.addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) ||
      instance.addExtension("VK_EXT_debug_report");
#endif
  vuloxr::vk::CheckVkResult(instance.create());

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .window = app->window,
  };
  VkSurfaceKHR surface;
  vuloxr::vk::CheckVkResult(
      vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface));

  auto [physicalDevice, presentFamily] = instance.pickPhysicalDevice(surface);
  assert(physicalDevice);

  // vuloxr::vk::Surface surface(instance, _surface, picked.physicalDevice);

  vuloxr::vk::Device device;
  device.layers = instance.layers;
  device.addExtension(*physicalDevice, "VK_KHR_swapchain");
  vuloxr::vk::CheckVkResult(device.create(instance, *physicalDevice,
                                          physicalDevice->graphicsFamilyIndex));

  vuloxr::vk::Swapchain swapchain(instance, surface, *physicalDevice,
                                  *presentFamily, device);
  swapchain.create(device.queueFamily);

  main_loop(
      [userdata, app]() {
        while (true) {
          if (!userdata->_active) {
            return false;
          }

          struct android_poll_source *source;
          int events;
          if (ALooper_pollOnce(
                  // timeout 0 for vulkan animation
                  0, nullptr, &events, (void **)&source) < 0) {
            return true;
          }
          if (source) {
            source->process(app, source);
          }
          if (app->destroyRequested) {
            return false;
          }
        }
      },
      instance, swapchain, *physicalDevice, device, nullptr);

  return true;
}

void android_main(android_app *app) {
  vuloxr::Logger::Verbose("Verbose");
  vuloxr::Logger::Info("Info");
  vuloxr::Logger::Warn("Warn");
  vuloxr::Logger::Error("Error");
#if NDEBUG
  bool useDebug = false;
  vuloxr::Logger::Info("## NDEBUG ##");
#else
  bool useDebug = true;
  vuloxr::Logger::Info("## DEBUG ##");
#endif

  vuloxr::android::UserData userdata{
      .pApp = app,
      ._appName = APP_NAME,
  };
  app->userData = &userdata;
  app->onAppCmd = vuloxr::android::UserData::on_app_cmd;
  app->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    auto is_exit = wait_window(app, &userdata);
    if (is_exit) {
      break;
    }

    is_exit = _main_loop(app, &userdata);
    if (is_exit) {
      break;
    }
  }
}

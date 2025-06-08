#include <assert.h>
#include <vko/android_userdata.h>
#include <vko/vko.h>

#include "main_loop.h"

auto APP_NAME = "hellotriangle";

static bool _main_loop(android_app *state, vko::UserData *userdata) {
  vko::Logger::Info("## main_loop");
  vko::Instance instance;
  instance.appInfo.pApplicationName = "Mali SDK";
  instance.appInfo.pEngineName = "Mali SDK";
  instance.instanceExtensions = {"VK_KHR_surface", "VK_KHR_android_surface"};
#ifdef NDEBUG
#else
  instance.validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instance.instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  instance.instanceExtensions.push_back("VK_EXT_debug_report");
#endif
  VKO_CHECK(instance.create());

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .window = state->window,
  };
  VkSurfaceKHR _surface;
  VKO_CHECK(vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &_surface));

  vko::PhysicalDevice picked = instance.pickPhysicalDevice(_surface);
  assert(picked.physicalDevice);

  vko::Surface surface(instance, _surface, picked.physicalDevice);

  vko::Device device;
  device.validationLayers = instance.validationLayers;
  VKO_CHECK(device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                          picked.presentFamilyIndex));

  main_loop(
      [userdata, state]() {
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
            source->process(state, source);
          }
          if (state->destroyRequested) {
            return false;
          }
        }
      },
      surface, picked, device, state->activity->assetManager);

  return true;
}

void android_main(android_app *state) {

#ifdef NDEBUG
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [release][android_main] ####");
#else
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [debug][android_main] ####");
#endif

  vko::UserData userdata{
      .pApp = state,
      ._appName = APP_NAME,
  };
  state->userData = &userdata;
  state->onAppCmd = vko::UserData::on_app_cmd;
  state->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    auto is_exit = wait_window(state, &userdata);
    if (is_exit) {
      break;
    }

    is_exit = _main_loop(state, &userdata);
    if (is_exit) {
      break;
    }
  }
}

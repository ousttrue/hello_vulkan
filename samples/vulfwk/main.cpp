#include "vulfwk.h"
#include <vko.h>

#ifdef ANDROID
#include <android_native_app_glue.h>

template <typename T> struct UserData {
  struct android_app *pApp = nullptr;
  T *impl = nullptr;
  bool active = false;
};

static void engineHandleCmd(android_app *pApp, int32_t cmd) {
  auto userData = static_cast<UserData<VulkanFramework> *>(pApp->userData);

  switch (cmd) {
  case APP_CMD_RESUME: {
    vko::Logger::Info("Resuming swapchain!\n");
    userData->active = true;
    break;
  }

  case APP_CMD_PAUSE: {
    vko::Logger::Info("Pausing swapchain!\n");
    userData->active = false;
    break;
  }

  case APP_CMD_INIT_WINDOW: {
    vko::Logger::Info("Initializing platform!\n");
    auto vulfwk = userData->impl;

    std::vector<const char *> validationLayers;
    std::vector<const char *> instanceExtensions = {"VK_KHR_surface",
                                                    "VK_KHR_android_surface"};
    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef NDEBUG
#else
    validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    if (!vulfwk->initializeInstance(validationLayers, instanceExtensions)) {
      vko::Logger::Error("failed: initializeInstance");
      return;
    }

    VkAndroidSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .flags = 0,
        .window = pApp->window,
    };
    VkSurfaceKHR surface;
    if (vkCreateAndroidSurfaceKHR(vulfwk->Instance, &info, nullptr, &surface) !=
        VK_SUCCESS) {
      vko::Logger::Error("failed: vkCreateAndroidSurfaceKHR");
      return;
    }

    if (!vulfwk->initializeDevice(validationLayers, deviceExtensions,
                                  surface)) {
      vko::Logger::Error("failed: initializeDevice");
      return;
    }
    vko::Logger::Info("vulkan initilized");
    auto width = ANativeWindow_getWidth(pApp->window);
    auto height = ANativeWindow_getHeight(pApp->window);
    vulfwk->drawFrame(width, height);

    break;
  }

  case APP_CMD_TERM_WINDOW:
    vko::Logger::Info("Terminating application!\n");
    // userData->impl->onTerminateWindow();
    break;
  }
}

void android_main(android_app *state) {
#ifdef NDEBUG
  vko::Logger::Info("[release]Entering android_main()!\n");
#else
  vko::Logger::Info("[debug]Entering android_main()!\n");
#endif

  VulkanFramework vulfwk("vulfwk", "No Engine");
  UserData<VulkanFramework> userData = {
      .pApp = state,
      .impl = &vulfwk,
  };
  state->userData = &userData;
  state->onAppCmd = engineHandleCmd;

  vulfwk.setAssetManager(state->activity->assetManager);

  for (;;) {
    struct android_poll_source *source;
    int ident;
    int events;

    while ((ident = ALooper_pollOnce(userData.active ? 0 : -1, nullptr, &events,
                                     (void **)&source)) >= 0) {
      if (source)
        source->process(state, source);

      if (state->destroyRequested)
        return;
    }

    // renderFrame
    if (userData.active && state->window) {
      // auto width = ANativeWindow_getWidth(state->window);
      // auto height = ANativeWindow_getHeight(state->window);
      // vulfwk.drawFrame(width, height);
    }
  }
}

#else
#include "platform_win32.h"

int main(int argc, char **argv) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifndef NDEBUG
  vko::Logger::Info("[debug build]");
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  Glfw glfw;
  auto hwnd = glfw.createWindow(640, 480, "vulfwk");
  glfw.addRequiredExtensions(instanceExtensions);

  VulkanFramework vulfwk("vulfwk", "No Engine");
  if (!vulfwk.initializeInstance(validationLayers, instanceExtensions)) {
    return 1;
  }

  VkSurfaceKHR surface;
  VKO_CHECK(glfwCreateWindowSurface(vulfwk.Instance, glfw._window, nullptr,
                                   &surface));

  if (!vulfwk.initializeDevice(validationLayers, deviceExtensions, surface)) {
    return 3;
  }

  while (true) {
    int width, height;
    if (!glfw.nextFrame(&width, &height)) {
      break;
    }

    vulfwk.drawFrame(width, height);
    // glfw.flush();
  }

  return 0;
}
#endif

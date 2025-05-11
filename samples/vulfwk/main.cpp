#include "logger.h"
#include "vulfwk.h"

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
    LOGI("Resuming swapchain!\n");
    userData->active = true;
    break;
  }

  case APP_CMD_PAUSE: {
    LOGI("Pausing swapchain!\n");
    userData->active = false;
    break;
  }

  case APP_CMD_INIT_WINDOW:
    LOGI("Initializing platform!\n");
    // userData->impl->onInitWindow(pApp->window);
    break;

  case APP_CMD_TERM_WINDOW:
    LOGI("Terminating application!\n");
    // userData->impl->onTerminateWindow();
    break;
  }
}

void android_main(android_app *state) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
#ifdef NDEBUG
  LOGI("[release]Entering android_main()!\n");
#else
  LOGI("[debug]Entering android_main()!\n");
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  VulkanFramework vulfwk("vulfwk", "No Engine");
  if (!vulfwk.initializeInstance(validationLayers, instanceExtensions)) {
    return;
  }

  UserData<VulkanFramework> userData = {
      .pApp = state,
      .impl = &vulfwk,
  };
  state->userData = &userData;
  state->onAppCmd = engineHandleCmd;

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

    if (userData.active) {
    }
  }
}

#else
#include "platform_win32.h"

int main(int argc, char **argv) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
#ifndef NDEBUG
  LOGI("[debug build]");
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

  if (!vulfwk.createSurfaceWin32(hwnd, GetModuleHandle(nullptr))) {
    return 2;
  }

  if (!vulfwk.initializeDevice()) {
    return 3;
  }

  while (true) {
    int width, height;
    if (!glfw.nextFrame(&width, &height)) {
      break;
    }
    // vulfwk.drawFrame();
    glfw.flush();
  }

  return 0;
}
#endif

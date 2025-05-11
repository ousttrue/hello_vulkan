#include "logger.h"
#include "vulfwk.h"

#ifdef ANDROID
#include <android_native_app_glue.h>
void android_main(android_app *state) { LOGI("Entering android_main()!\n"); }
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
  glfw.createWindow(640, 480, "vulfwk");
  glfw.addRequiredExtensions(instanceExtensions);

  VulkanFramework vulfwk("vulfwk", "No Engine");
  if (!vulfwk.initialize(validationLayers, instanceExtensions)) {
    LOGE("failed");
    return 1;
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

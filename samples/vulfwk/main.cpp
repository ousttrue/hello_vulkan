#include "logger.h"
#include "platform_win32.h"
#include "vulfwk.h"

int main(int argc, char **argv) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
  void *pNext = nullptr;

#ifndef NDEBUG
  LOGI("[debug build]");
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  pNext = getDebugMessengerCreateInfo(instanceExtensions);
#endif

  Glfw glfw;
  glfw.createWindow(640, 480, "vulfwk");
  glfw.addRequiredExtensions(instanceExtensions);

  VulkanFramework vulfwk("vulfwk", "No Engine");
  if (!vulfwk.initialize(validationLayers, pNext, instanceExtensions)) {
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

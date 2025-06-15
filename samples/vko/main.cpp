#include "main_loop.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

auto NAME = "vko";

class Glfw {

public:
  GLFWwindow *_window = nullptr;
  Glfw() {
    glfwSetErrorCallback([](int error, const char *description) {
      vko::Logger::Error("GLFW Error %d: %s\n", error, description);
    });
    glfwInit();
  }
  ~Glfw() {
    if (_window) {
      glfwDestroyWindow(_window);
    }
    glfwTerminate();
  }
  void *createWindow(int width, int height, const char *title) {
    if (!glfwVulkanSupported()) {
      vko::Logger::Error("GLFW: Vulkan Not Supported\n");
      return nullptr;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    return glfwGetWin32Window(_window);
  }
  bool nextFrame() {
    if (glfwWindowShouldClose(_window)) {
      return false;
    }
    glfwPollEvents();
    // glfwGetFramebufferSize(_window, width, height);
    return true;
  }
};

int main(int argc, char **argv) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
#ifndef NDEBUG
  vko::Logger::Info("[debug build]");
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  Glfw glfw;
  auto hwnd = glfw.createWindow(640, 480, NAME);
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
    instanceExtensions.push_back(glfwExtensions[i]);
  }

  vko::Instance instance;
  instance.validationLayers = validationLayers;
  instance.instanceExtensions = instanceExtensions;
  instance.appInfo.pApplicationName = NAME;
  instance.appInfo.pEngineName = "No Engine";
  VKO_CHECK(instance.create());

  VkSurfaceKHR _surface;
  VKO_CHECK(
      glfwCreateWindowSurface(instance, glfw._window, nullptr, &_surface));

  auto physicalDevice = instance.pickPhysicalDevice(_surface);
  vko::Surface surface(instance, _surface, physicalDevice.physicalDevice);

  vko::Device device;
  device.validationLayers = instance.validationLayers;
  device.deviceExtensions = deviceExtensions;
  VKO_CHECK(device.create(physicalDevice.physicalDevice,
                          physicalDevice.graphicsFamilyIndex,
                          physicalDevice.presentFamilyIndex));

  main_loop([&glfw]() { return glfw.nextFrame(); }, surface, physicalDevice,
            device);

  return 0;
}

#include <vko/vko.h>

#include "main_loop.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

class Glfw {

public:
  GLFWwindow *_window = nullptr;
  Glfw() { glfwInit(); }
  ~Glfw() {
    if (_window) {
      glfwDestroyWindow(_window);
    }
    glfwTerminate();
  }
  void *createWindow(int width, int height, const char *title) {
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

//   glfwSetErrorCallback([](int error, const char *description) {
//     vko::Logger::Error("GLFW Error %d: %s\n", error, description);
//   });
//   if (!glfwInit()) {
//     return 1;
//   }
//   // Create window with Vulkan context
//   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//   GLFWwindow *window =
//       glfwCreateWindow(1280, 720, WINDOW_TITLE, nullptr, nullptr);
//   if (!glfwVulkanSupported()) {
//     printf("GLFW: Vulkan Not Supported\n");
//     return 1;
//   }
//
//   {
//     //
//     // vulkan instance with debug layer
//     //
//     vko::Instance instance;
//     // vulkan extension for glfw surface
//     uint32_t extensions_count = 0;
//     auto glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
//     for (uint32_t i = 0; i < extensions_count; i++) {
//       instance.instanceExtensions.push_back(glfw_extensions[i]);
//     }
// #ifndef NDEBUG
//     instance.validationLayers.push_back("VK_LAYER_KHRONOS_validation");
//     instance.instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
// #endif
//     VKO_CHECK(instance.create());
//
//     //
//     // vulkan device with glfw surface
//     //
//     VkSurfaceKHR _surface;
//     VKO_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &_surface));
//     auto picked = instance.pickPhysicalDevice(_surface);
//     vko::Surface surface(instance, _surface, picked.physicalDevice);
//
//     vko::Device device;
//     device.validationLayers = instance.validationLayers;
//     VKO_CHECK(device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
//                             picked.presentFamilyIndex));


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
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
    instanceExtensions.push_back(glfwExtensions[i]);
  }

  vko::Instance instance;
  instance.validationLayers = validationLayers;
  instance.instanceExtensions = instanceExtensions;
  instance.appInfo.pApplicationName = "vulfwk";
  instance.appInfo.pEngineName = "No Engine";
  VKO_CHECK(instance.create());

  VkSurfaceKHR _surface;
  VKO_CHECK(
      glfwCreateWindowSurface(instance, glfw._window, nullptr, &_surface));

  auto picked = instance.pickPhysicalDevice(_surface);
  vko::Surface surface(instance, _surface, picked.physicalDevice);

  vko::Device device;
  device.validationLayers = instance.validationLayers;
  VKO_CHECK(device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                          picked.presentFamilyIndex));

  main_loop([&glfw]() { return glfw.nextFrame(); }, surface, picked, device);

  return 0;
}

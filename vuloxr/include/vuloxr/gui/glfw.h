#pragma once
#include "../gui.h"
#include "../vk.h"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #define GLFW_EXPOSE_NATIVE_WIN32
// #include <GLFW/glfw3native.h>

#include "vuloxr.h"

namespace vuloxr {
namespace gui {

class Glfw {
  GLFWwindow *window = nullptr;

  static void glfw_error_callback(int error, const char *description) {
    Logger::Error("GLFW Error %d: %s\n", error, description);
  }

public:
  Glfw() {
    glfwSetErrorCallback(glfw_error_callback);
    assert(glfwInit());
  }

  ~Glfw() {
    if (window) {
      glfwDestroyWindow(window);
    }
    glfwTerminate();
  }

  GLFWwindow *createWindow(int width, int height, const char *windowTitle) {
    //     if (!glfwVulkanSupported()) {
    //       vko::Logger::Error("GLFW: Vulkan Not Supported\n");
    //       return nullptr;
    //     }
    // Create window with Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    //     // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    this->window =
        glfwCreateWindow(width, height, windowTitle, nullptr, nullptr);
    if (!glfwVulkanSupported()) {
      printf("GLFW: Vulkan Not Supported\n");
      return nullptr;
    }
    return this->window;
  }

  //     return glfwGetWin32Window(_window);

  std::optional<WindowState> newFrame() const {
    if (glfwWindowShouldClose(this->window)) {
      return std::nullopt;
    }
    glfwPollEvents();
    WindowState state;

    int w, h;
    glfwGetFramebufferSize(this->window, &w, &h);
    state.width = w;
    state.height = h;

    double x, y;
    glfwGetCursorPos(this->window, &x, &y);
    state.mouse.x = x;
    state.mouse.y = y;
    state.mouse.leftDown =
        glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_LEFT);
    state.mouse.middleDown =
        glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_MIDDLE);
    state.mouse.rightDown =
        glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_RIGHT);

    return state;
  }

  WindowLoopOnce makeWindowLoopOnce() const {
    return std::bind(&Glfw::newFrame, this);
  }

  std::tuple<int, int> framebufferSize() const {
    int w, h;
    glfwGetFramebufferSize(this->window, &w, &h);
    return {w, h};
  }

  bool isIconified() const {
    return glfwGetWindowAttrib(this->window, GLFW_ICONIFIED) != 0;
  }

  struct VulkanResources {
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Swapchain swapchain;
  };
  VulkanResources createVulkan(bool useDebug) {
    vk::Instance instance;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
      instance.addExtension(glfwExtensions[i]);
    }
    if (useDebug) {
      instance.addLayer("VK_LAYER_KHRONOS_validation");
      if (!instance.addExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        // fallback:
        // https://developer.android.com/ndk/guides/graphics/validation-layer
        instance.addExtension("VK_EXT_debug_report");
      }
    }
    // instance.appInfo.pApplicationName = NAME;
    // instance.appInfo.pEngineName = "No Engine";
    vk::CheckVkResult(instance.create());

    VkSurfaceKHR surface;
    vk::CheckVkResult(
        glfwCreateWindowSurface(instance, this->window, nullptr, &surface));

    auto [physicalDevice, presentFamilyIndex] =
        instance.pickPhysicalDevice(surface);
    assert(physicalDevice);
    assert(presentFamilyIndex);
    // vk::Surface surface(instance, _surface, *physicalDevice);

    vk::Device device;
    device.layers = instance.layers;
    device.addExtension(*physicalDevice, "VK_KHR_swapchain");
    vk::CheckVkResult(device.create(instance, *physicalDevice,
                                    physicalDevice->graphicsFamilyIndex));

    vk::Swapchain swapchain(instance, surface, *physicalDevice,
                            *presentFamilyIndex, device, device.queueFamily);
    swapchain.create();

    return {std::move(instance), *physicalDevice, std::move(device),
            std::move(swapchain)};
  }
};

} // namespace gui
} // namespace vuloxr

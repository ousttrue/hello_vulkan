#pragma once
#include "vuloxr/vk.h"
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #define GLFW_EXPOSE_NATIVE_WIN32
// #include <GLFW/glfw3native.h>

#include "vuloxr.h"

namespace vuloxr {
namespace glfw {

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

  bool newFrame() const {
    if (glfwWindowShouldClose(this->window)) {
      return false;
    }
    glfwPollEvents();
    return true;
  }

  std::tuple<int, int> framebufferSize() const {
    int w, h;
    glfwGetFramebufferSize(this->window, &w, &h);
    return {w, h};
  }

  bool isIconified() const {
    return glfwGetWindowAttrib(this->window, GLFW_ICONIFIED) != 0;
  }
};

} // namespace glfw
} // namespace vuloxr

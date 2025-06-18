#pragma once
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../vuloxr.h"

namespace vuloxr {

class Glfw {
  GLFWwindow *window = nullptr;

  static void glfw_error_callback(int error, const char *description) {
    Logger::Error("GLFW Error %d: %s\n", error, description);
  }

public:
  Glfw() { glfwSetErrorCallback(glfw_error_callback); }
  ~Glfw() {
    if (window) {
      glfwDestroyWindow(window);
    }
    glfwTerminate();
  }

  GLFWwindow *createWindow() {
    if (!glfwInit()) {
      return nullptr;
    }

    // Create window with Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    this->window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+Vulkan example",
                                    nullptr, nullptr);
    if (!glfwVulkanSupported()) {
      printf("GLFW: Vulkan Not Supported\n");
      return nullptr;
    }
    return this->window;
  }

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

} // namespace vuloxr

#include "platform_win32.h"

Glfw::Glfw() { glfwInit(); }

Glfw::~Glfw() {
  if (_window) {
    glfwDestroyWindow(_window);
  }
  glfwTerminate();
}

void Glfw::createWindow(int width, int height, const char *title) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  _window = glfwCreateWindow(width, height, title, nullptr, nullptr);
}

void Glfw::addRequiredExtensions(std::vector<const char *> &extensions) {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
    extensions.push_back(glfwExtensions[i]);
  }
}

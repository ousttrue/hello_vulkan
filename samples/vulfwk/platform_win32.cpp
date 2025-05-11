#include "platform_win32.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

Glfw::Glfw() { glfwInit(); }

Glfw::~Glfw() {
  if (_window) {
    glfwDestroyWindow(_window);
  }
  glfwTerminate();
}

void *Glfw::createWindow(int width, int height, const char *title) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  _window = glfwCreateWindow(width, height, title, nullptr, nullptr);

  return glfwGetWin32Window(_window);
}

void Glfw::addRequiredExtensions(std::vector<const char *> &extensions) {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
    extensions.push_back(glfwExtensions[i]);
  }
}

bool Glfw::nextFrame(int *width, int *height) {
  if (glfwWindowShouldClose(_window)) {
    return false;
  }
  glfwPollEvents();
  glfwGetFramebufferSize(_window, width, height);
  return true;
}

void Glfw::flush() { glfwSwapBuffers(_window); }

#include "glfw_app.h"

#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>

static bool _framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  // auto app = reinterpret_cast<GlfwApp *>(glfwGetWindowUserPointer(window));
  _framebufferResized = true;
}

std::vector<const char *>
GlfwApp::getRequiredExtensions(bool enableValidationLayers) {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> extensions(glfwExtensions,
                                       glfwExtensions + glfwExtensionCount);

  if (enableValidationLayers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

GlfwApp::GlfwApp(void *window) : _window(window) {
  glfwSetWindowUserPointer((GLFWwindow *)_window, this);
  glfwSetFramebufferSizeCallback((GLFWwindow *)_window,
                                 framebufferResizeCallback);
}

GlfwApp::~GlfwApp() {
  glfwDestroyWindow((GLFWwindow *)_window);
  glfwTerminate();
}

std::shared_ptr<GlfwApp> GlfwApp::initWindow(int width, int height) {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  auto window =
      glfwCreateWindow(width, height, "Vulkan Triangle", nullptr, nullptr);

  auto ptr = std::shared_ptr<GlfwApp>(new GlfwApp(window));
  return ptr;
}

VkSurfaceKHR GlfwApp::createSurface(VkInstance instance) {
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(instance, (GLFWwindow *)_window, nullptr,
                              &surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
  return surface;
}

bool GlfwApp::running(int *pwidth, int *pheight, bool *presized) {
  if (glfwWindowShouldClose((GLFWwindow *)_window)) {
    return false;
  }
  glfwPollEvents();
  if (pwidth && pheight) {
    glfwGetFramebufferSize((GLFWwindow *)_window, pwidth, pheight);
  }
  if (presized) {
    *presized = _framebufferResized;
  }
  _framebufferResized = false;

  return true;
}

std::tuple<int, int> GlfwApp::getSize() const {
  int width, height;
  glfwGetFramebufferSize((GLFWwindow *)_window, &width, &height);
  return {width, height};
}

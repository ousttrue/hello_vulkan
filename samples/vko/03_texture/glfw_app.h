#pragma once
#include <vulkan/vulkan_core.h>

#include <memory>
#include <vector>

class GlfwApp {
  void *_window;

  GlfwApp(void *window);

public:
  ~GlfwApp();
  static std::shared_ptr<GlfwApp> initWindow(int width, int height);
  VkSurfaceKHR createSurface(VkInstance instance);
  bool running(int *pwidth = {}, int *pheight = {}, bool *presized = {});
  std::tuple<int, int> getSize() const;

  static std::vector<const char *>
  getRequiredExtensions(bool enableValidationLayers);
};

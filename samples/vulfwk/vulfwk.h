#pragma once
#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

class VulkanFramework {
  std::string _appName;
  std::string _engineName;
  VkInstance Instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;

public:
  VulkanFramework(const char *appName, const char *engineName);
  ~VulkanFramework();

  bool initialize(const std::vector<const char *> &layerNames,
                  const std::vector<const char *> &extensionNames);

  void cleanup();

private:
  bool createInstance(const std::vector<const char *> &layerNames,
                      const std::vector<const char *> &extensionNames);
};

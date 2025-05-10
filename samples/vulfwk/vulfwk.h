#pragma once
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

class VulkanFramework {
  std::string _appName;
  std::string _engineName;
  VkApplicationInfo ApplicationInfo{};
  VkInstanceCreateInfo InstanceCreateInfo{};
  VkInstance Instance = VK_NULL_HANDLE;

public:
  VulkanFramework(const char *appName, const char *engineName);
  ~VulkanFramework();

  bool initialize(const std::vector<const char *> &layerNames,
                  const void *pNext,
                  const std::vector<const char *> &extensionNames);

  void cleanup();

private:
  bool createInstance(const std::vector<const char *> &layerNames,
                      const void *pNext,
                      const std::vector<const char *> &extensionNames);
};

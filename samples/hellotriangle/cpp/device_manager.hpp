#pragma once
#include <vulkan/vulkan.h>

#include <vector>

struct PhysicalDevice {
  VkPhysicalDevice Gpu = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties Properties;
  VkPhysicalDeviceMemoryProperties MemoryProperties;
  std::vector<VkQueueFamilyProperties> QueueProperties;
  uint32_t SelectedQueueFamilyIndex = -1;

  bool SelectQueueFamily(VkPhysicalDevice gpu, VkSurfaceKHR surface);
};

struct DeviceManager {
  VkInstance Instance;
  VkSurfaceKHR Surface = VK_NULL_HANDLE;
  std::vector<VkPhysicalDevice> Gpus;
  std::vector<VkPhysicalDeviceProperties> GpuProps;
  PhysicalDevice Selected = {};
  // created logical device
  VkDevice Device = VK_NULL_HANDLE;
  VkQueue Queue = VK_NULL_HANDLE;

  VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;
  // PFN_vkDebugReportCallbackEXT externalDebugCallback = nullptr;
  // void *pExternalDebugCallbackUserData = nullptr;

  DeviceManager(VkInstance instance) : Instance(instance) {}
  ~DeviceManager();
  static std::shared_ptr<DeviceManager>
  create(const char *appName, const char *engineName,
         const std::vector<const char *> &layers);
  bool createSurfaceFromAndroid(ANativeWindow *window);
  VkPhysicalDevice selectGpu();
  bool createLogicalDevice(const std::vector<const char *> &layers);
};

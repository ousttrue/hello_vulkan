#pragma once
#include <vector>
#include <vulkan/vulkan.h>

VkDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT(VkInstance instance);
void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);

struct InstanceExtensionManager {
  std::vector<VkLayerProperties> InstanceLayers;
  std::vector<const char *> RequiredLayers;

  std::vector<VkExtensionProperties> InstanceExtensions;
  std::vector<const char *> RequiredExtensions;

public:
  InstanceExtensionManager(const std::vector<const char *> &requiredLayers,
                           const std::vector<const char *> &requiredExtensions);
  bool pushLayer(const char *layerName);
  bool pushExtension(const char *extensionName);
};

struct DeviceExtensionManager {
  std::vector<VkLayerProperties> DeviceLayers;
  std::vector<const char *> RequiredLayers;

  std::vector<VkExtensionProperties> DeviceExtensions;
  std::vector<const char *> RequiredExtensions;

public:
  DeviceExtensionManager(VkPhysicalDevice gpu,
                         const std::vector<const char *> &requiredLayers,
                         const std::vector<const char *> &requiredExtensions);
  bool pushLayer(const char *layerName);
  bool pushExtension(const char *extensionName);
};

#pragma once
#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

class SwapchainImpl;
class PipelineImpl;
class VulkanFramework {
  std::shared_ptr<SwapchainImpl> Swapchain;
  std::shared_ptr<PipelineImpl> Pipeline;

  std::string _appName;
  std::string _engineName;
  VkInstance Instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;
  VkSurfaceKHR Surface = VK_NULL_HANDLE;
  VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
  VkDevice Device = VK_NULL_HANDLE;
  VkQueue GraphicsQueue = VK_NULL_HANDLE;
  VkQueue PresentQueue = VK_NULL_HANDLE;

  VkFormat SwapchainImageFormat = {};

  void *AssetManager = nullptr;

public:
  VulkanFramework(const char *appName, const char *engineName);
  ~VulkanFramework();
  void setAssetManager(void *p) { AssetManager = p; }
  bool
  initializeInstance(const std::vector<const char *> &layerNames,
                     const std::vector<const char *> &instanceExtensionNames);
  void cleanup();
#ifdef ANDROID
  bool createSurfaceAndroid(void *window);
#else
  bool createSurfaceWin32(void *hInstance, void *hWnd);
#endif
  bool initializeDevice(const std::vector<const char *> &layerNames,
                        const std::vector<const char *> &deviceExtensionNames);

  bool drawFrame(uint32_t width, uint32_t height);

private:
  bool createInstance(const std::vector<const char *> &layerNames,
                      const std::vector<const char *> &instanceExtensionNames);
  bool
  pickPhysicalDevice(const std::vector<const char *> &deviceExtensionNames);
  bool
  createLogicalDevice(const std::vector<const char *> &layerNames,
                      const std::vector<const char *> &deviceExtensionNames);
};

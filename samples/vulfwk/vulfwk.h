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
  VkSurfaceKHR Surface = VK_NULL_HANDLE;
  VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
  VkDevice Device = VK_NULL_HANDLE;
  VkQueue GraphicsQueue = VK_NULL_HANDLE;
  VkQueue PresentQueue = VK_NULL_HANDLE;

  VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> SwapChainImages;
  VkFormat SwapchainImageFormat = {};
  VkExtent2D SwapchainExtent = {0, 0};
  std::vector<VkImageView> SwapchainImageViews;
  std::vector<VkFramebuffer> SwapchainFramebuffers;

public:
  VulkanFramework(const char *appName, const char *engineName);
  ~VulkanFramework();
  bool
  initializeInstance(const std::vector<const char *> &layerNames,
                     const std::vector<const char *> &instanceExtensionNames);
  void cleanup();
  bool createSurfaceWin32(void *hInstance, void *hWnd);
  bool initializeDevice(const std::vector<const char *> &layerNames,
                        const std::vector<const char *> &deviceExtensionNames);
  bool createSwapChain(VkExtent2D imageExtent);

private:
  bool createInstance(const std::vector<const char *> &layerNames,
                      const std::vector<const char *> &instanceExtensionNames);
  bool
  pickPhysicalDevice(const std::vector<const char *> &deviceExtensionNames);
  bool
  createLogicalDevice(const std::vector<const char *> &layerNames,
                      const std::vector<const char *> &deviceExtensionNames);
};

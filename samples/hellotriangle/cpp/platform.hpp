#pragma once
#include "per_frame.hpp"
#include "semaphore_manager.hpp"
#include <vulkan/vulkan.h>

namespace MaliSDK {

struct SwapchainDimensions {
  unsigned width;
  unsigned height;
  VkFormat format;
};

struct Platform {
  ANativeWindow *pNativeWindow = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice gpu = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties gpuProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;
  std::vector<VkQueueFamilyProperties> queueProperties;
  unsigned graphicsQueueIndex;
  std::vector<std::string> externalLayers;
  PFN_vkDebugReportCallbackEXT externalDebugCallback = nullptr;
  void *pExternalDebugCallbackUserData = nullptr;

  inline void
  addExternalLayers(std::vector<const char *> &activeLayers,
                    const std::vector<VkLayerProperties> &supportedLayers) {
    for (auto &layer : externalLayers) {
      for (auto &supportedLayer : supportedLayers) {
        if (layer == supportedLayer.layerName) {
          activeLayers.push_back(supportedLayer.layerName);
          LOGI("Found external layer: %s\n", supportedLayer.layerName);
          break;
        }
      }
    }
  }

  SemaphoreManager *semaphoreManager = nullptr;

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  SwapchainDimensions swapchainDimensions;
  std::vector<VkImage> swapchainImages;

  VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;

public:
  Platform() = default;
  ~Platform() { terminate(); }

  Platform(Platform &&) = delete;
  void operator=(Platform &&) = delete;

  void setNativeWindow(ANativeWindow *pWindow) { pNativeWindow = pWindow; }
  Result initialize();
  inline void addExternalLayer(const char *pName) {
    externalLayers.push_back(pName);
  }
  inline void setExternalDebugCallback(PFN_vkDebugReportCallbackEXT callback,
                                       void *pUserData) {
    externalDebugCallback = callback;
    pExternalDebugCallbackUserData = pUserData;
  }
  inline PFN_vkDebugReportCallbackEXT getExternalDebugCallback() const {
    return externalDebugCallback;
  }
  inline void *getExternalDebugCallbackUserData() const {
    return pExternalDebugCallbackUserData;
  }
  SwapchainDimensions getPreferredSwapchain() const {
    SwapchainDimensions chain = {1280, 720, VK_FORMAT_B8G8R8A8_UNORM};
    return chain;
  }

  Result createWindow(const SwapchainDimensions &swapchain) {
    return initVulkan(swapchain, {"VK_KHR_surface", "VK_KHR_android_surface"},
                      {"VK_KHR_swapchain"});
  }

  void getCurrentSwapchain(std::vector<VkImage> *images,
                           SwapchainDimensions *swapchain);
  unsigned getNumSwapchainImages() const { return swapchainImages.size(); }
  Result acquireNextImage(unsigned *index);
  Result presentImage(unsigned index);
  void terminate();
  inline VkDevice getDevice() const { return device; }
  inline VkPhysicalDevice getPhysicalDevice() const { return gpu; }
  inline VkInstance getInstance() const { return instance; }
  inline VkQueue getGraphicsQueue() const { return queue; }
  inline unsigned getGraphicsQueueIndex() const { return graphicsQueueIndex; }
  inline const VkPhysicalDeviceProperties &getGpuProperties() const {
    return gpuProperties;
  }
  inline const VkPhysicalDeviceMemoryProperties &getMemoryProperties() const {
    return memoryProperties;
  }
  void destroySwapchain();
  Result initSwapchain(const SwapchainDimensions &swapchain);
  Result initVulkan(const SwapchainDimensions &swapchain,
                    const std::vector<const char *> &instanceExtensions,
                    const std::vector<const char *> &deviceExtensions);
  bool validateExtensions(const std::vector<const char *> &required,
                          const std::vector<VkExtensionProperties> &available);
  VkSurfaceKHR createSurface();

  void onPause() { destroySwapchain(); }
  void onResume(const SwapchainDimensions &swapchain) {
    vkDeviceWaitIdle(device);
    initSwapchain(swapchain);
  }

  Result onPlatformUpdate();
  void waitIdle() { vkDeviceWaitIdle(device); }
  std::vector<std::unique_ptr<PerFrame>> perFrame;
  unsigned swapchainIndex = 0;
  unsigned renderingThreadCount = 0;
  VkCommandBuffer requestPrimaryCommandBuffer() {
    return perFrame[swapchainIndex]->commandManager.requestCommandBuffer();
  }
  const VkSemaphore &getSwapchainReleaseSemaphore() const {
    return perFrame[swapchainIndex]->swapchainReleaseSemaphore;
  }
  const VkSemaphore &getSwapchainAcquireSemaphore() const {
    return perFrame[swapchainIndex]->swapchainAcquireSemaphore;
  }
  FenceManager &getFenceManager() {
    return perFrame[swapchainIndex]->fenceManager;
  }
  void submitSwapchain(VkCommandBuffer cmdBuffer);
  void submitCommandBuffer(VkCommandBuffer, VkSemaphore acquireSemaphore,
                           VkSemaphore releaseSemaphore);
  VkSemaphore beginFrame(unsigned index, VkSemaphore acquireSemaphore) {
    swapchainIndex = index;
    perFrame[swapchainIndex]->beginFrame();
    return perFrame[swapchainIndex]->setSwapchainAcquireSemaphore(
        acquireSemaphore);
  }
  void setRenderingThreadCount(unsigned count) {
    vkQueueWaitIdle(queue);
    for (auto &pFrame : perFrame)
      pFrame->setSecondaryCommandManagersCount(count);
    renderingThreadCount = count;
  }
};

} // namespace MaliSDK

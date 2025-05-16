#pragma once
#include "backbuffer.hpp"
#include "per_frame.hpp"
#include "semaphore_manager.hpp"
#include <vulkan/vulkan.h>

struct SwapchainDimensions {
  unsigned width;
  unsigned height;
  VkFormat format;
};

class Platform {
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
  VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;

  MaliSDK::SemaphoreManager *semaphoreManager = nullptr;

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> swapchainImages;

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

  Platform() = default;

  SwapchainDimensions swapchainDimensions;
  std::vector<std::shared_ptr<Backbuffer>> backbuffers;

public:
  ~Platform();
  Platform(Platform &&) = delete;
  void operator=(Platform &&) = delete;
  static std::shared_ptr<Platform> create(ANativeWindow *window);

  VkFormat surfaceFormat() const { return swapchainDimensions.format; }
  std::shared_ptr<Backbuffer> getBackbuffer(uint32_t index) const {
    return backbuffers[index];
  }
  inline VkDevice getDevice() const { return device; }
  inline const VkPhysicalDeviceMemoryProperties &getMemoryProperties() const {
    return memoryProperties;
  }
  void updateSwapchain(VkRenderPass renderPass);
  VkCommandBuffer beginRender(const std::shared_ptr<Backbuffer> &backbuffer);
  void submitSwapchain(VkCommandBuffer cmdBuffer);
  MaliSDK::Result presentImage(unsigned index);
  MaliSDK::Result acquireNextImage(unsigned *index);
  SwapchainDimensions getSwapchainDimesions() const {
    return swapchainDimensions;
  }

private:
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

  unsigned getNumSwapchainImages() const { return swapchainImages.size(); }
  inline VkPhysicalDevice getPhysicalDevice() const { return gpu; }
  inline VkInstance getInstance() const { return instance; }
  inline VkQueue getGraphicsQueue() const { return queue; }
  inline unsigned getGraphicsQueueIndex() const { return graphicsQueueIndex; }
  inline const VkPhysicalDeviceProperties &getGpuProperties() const {
    return gpuProperties;
  }
  void destroySwapchain();
  MaliSDK::Result initSwapchain();
  MaliSDK::Result initVulkan(ANativeWindow *window);
  bool validateExtensions(const std::vector<const char *> &required,
                          const std::vector<VkExtensionProperties> &available);

  void onPause() { destroySwapchain(); }

  MaliSDK::Result onPlatformUpdate();
  void waitIdle() { vkDeviceWaitIdle(device); }
  std::vector<std::unique_ptr<MaliSDK::PerFrame>> perFrame;
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
  MaliSDK::FenceManager &getFenceManager() {
    return perFrame[swapchainIndex]->fenceManager;
  }
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

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
  std::shared_ptr<class DeviceManager> _device;

  MaliSDK::SemaphoreManager *semaphoreManager = nullptr;

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> swapchainImages;

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
  VkDevice getDevice() const;
  const VkPhysicalDeviceMemoryProperties &getMemoryProperties() const;
  void updateSwapchain(VkRenderPass renderPass);
  VkCommandBuffer beginRender(const std::shared_ptr<Backbuffer> &backbuffer);
  void submitSwapchain(VkCommandBuffer cmdBuffer);
  MaliSDK::Result presentImage(unsigned index);
  MaliSDK::Result acquireNextImage(unsigned *index);
  SwapchainDimensions getSwapchainDimesions() const {
    return swapchainDimensions;
  }

private:
  unsigned getNumSwapchainImages() const { return swapchainImages.size(); }
  void destroySwapchain();
  MaliSDK::Result initSwapchain(const VkSurfaceCapabilitiesKHR &surfaceProperties);
  MaliSDK::Result initVulkan(ANativeWindow *window);

  void onPause() { destroySwapchain(); }

  MaliSDK::Result onPlatformUpdate();
  // void waitIdle() { vkDeviceWaitIdle(device); }
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
  void setRenderingThreadCount(unsigned count) ;
};

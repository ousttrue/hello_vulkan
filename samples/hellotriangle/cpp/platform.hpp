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

struct SwapchainManager {
  VkDevice Device;
  VkQueue PresentationQueue;
  VkSwapchainKHR Swapchain;
  SwapchainDimensions swapchainDimensions;

  std::vector<VkImage> swapchainImages;
  std::vector<std::shared_ptr<Backbuffer>> backbuffers;
  std::vector<std::unique_ptr<MaliSDK::PerFrame>> perFrame;
  unsigned swapchainIndex = 0;
  unsigned renderingThreadCount = 0;

  SwapchainManager(VkDevice device, VkQueue presentaionQueue,
                   VkSwapchainKHR swapchain)
      : Device(device), PresentationQueue(presentaionQueue),
        Swapchain(swapchain) {}
  ~SwapchainManager();
  static std::shared_ptr<SwapchainManager>
  create(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkDevice device,
         VkQueue presentaionQueue, VkSwapchainKHR oldSwapchain);

  // VkFormat surfaceFormat() const { return swapchainDimensions.format; }
  std::shared_ptr<Backbuffer> getBackbuffer(uint32_t index) const {
    return backbuffers[index];
  }
  SwapchainDimensions getSwapchainDimesions() const {
    return swapchainDimensions;
  }
  void submitCommandBuffer(VkQueue graphicsQueue, VkCommandBuffer,
                           VkSemaphore acquireSemaphore,
                           VkSemaphore releaseSemaphore);
  VkSemaphore beginFrame(unsigned index, VkSemaphore acquireSemaphore) {
    swapchainIndex = index;
    perFrame[swapchainIndex]->beginFrame();
    return perFrame[swapchainIndex]->setSwapchainAcquireSemaphore(
        acquireSemaphore);
  }
  void setRenderingThreadCount(unsigned count);

  bool presentImage(unsigned index);
};

class Platform {
  MaliSDK::SemaphoreManager *semaphoreManager = nullptr;

  Platform() = default;

public:
  std::shared_ptr<class DeviceManager> _device;
  std::shared_ptr<class SwapchainManager> _swapchain;
  ~Platform();
  Platform(Platform &&) = delete;
  void operator=(Platform &&) = delete;
  static std::shared_ptr<Platform> create(ANativeWindow *window);

  VkDevice getDevice() const;
  const VkPhysicalDeviceMemoryProperties &getMemoryProperties() const;

  VkCommandBuffer beginRender(const std::shared_ptr<Backbuffer> &backbuffer);
  void submitSwapchain(VkCommandBuffer cmdBuffer);
  MaliSDK::Result acquireNextImage(unsigned *index, VkRenderPass renderPass);

private:
  MaliSDK::Result initSwapchain(VkRenderPass renderPass);
  MaliSDK::Result initVulkan(ANativeWindow *window);
};

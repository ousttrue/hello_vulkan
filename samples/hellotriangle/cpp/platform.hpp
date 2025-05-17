#pragma once
#include "common.hpp"

#include <vulkan/vulkan.h>

#include <memory>

class Platform {
  std::shared_ptr<class SemaphoreManager> semaphoreManager;

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

  std::shared_ptr<class Backbuffer> getBackbuffer(VkRenderPass renderPass);

private:
  MaliSDK::Result acquireNextImage(unsigned *index, VkRenderPass renderPass);
  MaliSDK::Result initSwapchain(VkRenderPass renderPass);
  MaliSDK::Result initVulkan(ANativeWindow *window);
};

#pragma once
#include "backbuffer.hpp"
#include "per_frame.hpp"

#include <vulkan/vulkan.h>

#include <vector>

struct SwapchainDimensions {
  unsigned width;
  unsigned height;
  VkFormat format;
};

class SwapchainManager {
  VkDevice Device;
  VkQueue GraphicsQueue;
  VkQueue PresentationQueue;
  VkSwapchainKHR Swapchain;
  SwapchainDimensions swapchainDimensions;

  std::vector<VkImage> swapchainImages;
  std::vector<std::shared_ptr<Backbuffer>> backbuffers;
  std::vector<std::unique_ptr<PerFrame>> perFrame;
  unsigned swapchainIndex = 0;
  unsigned renderingThreadCount = 0;

  SwapchainManager(VkDevice device, VkQueue graphicsQueue,
                   VkQueue presentaionQueue, VkSwapchainKHR swapchain)
      : Device(device), GraphicsQueue(graphicsQueue),
        PresentationQueue(presentaionQueue), Swapchain(swapchain) {}

public:
  ~SwapchainManager();
  static std::shared_ptr<SwapchainManager>
  create(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkDevice device,
         uint32_t graphicsQueueIndex, VkQueue graphicsQueue,
         VkQueue presentaionQueue, VkRenderPass renderPass,
         VkSwapchainKHR oldSwapchain);
  VkSwapchainKHR Handle() const { return Swapchain; }

  SwapchainDimensions getSwapchainDimesions() const {
    return swapchainDimensions;
  }

  std::shared_ptr<Backbuffer> getBackbuffer(uint32_t index) const {
    return backbuffers[index];
  }

private:
  void submitCommandBuffer(VkCommandBuffer, VkSemaphore acquireSemaphore,
                           VkSemaphore releaseSemaphore);
  void setRenderingThreadCount(unsigned count);

public:
  VkSemaphore beginFrame(unsigned index, VkSemaphore acquireSemaphore) {
    swapchainIndex = index;
    perFrame[swapchainIndex]->beginFrame();
    return perFrame[swapchainIndex]->setSwapchainAcquireSemaphore(
        acquireSemaphore);
  }
  VkCommandBuffer beginRender(const std::shared_ptr<Backbuffer> &backbuffer);
  void submitSwapchain(VkCommandBuffer cmdBuffer);
  bool presentImage(unsigned index);
};

#pragma once
#include "backbuffer.hpp"
#include "per_frame.hpp"

#include <vulkan/vulkan.h>

#include <vector>

class SwapchainManager {
  VkDevice _device;
  VkQueue GraphicsQueue;
  VkQueue PresentationQueue;
  VkSwapchainKHR Swapchain;
  // SwapchainDimensions swapchainDimensions;
  VkExtent2D _size;
  VkFormat _format;

  std::vector<VkImage> swapchainImages;
  std::vector<std::shared_ptr<Backbuffer>> backbuffers;
  std::vector<std::unique_ptr<PerFrame>> perFrame;
  unsigned swapchainIndex = 0;
  unsigned renderingThreadCount = 0;

  SwapchainManager(VkDevice device, VkQueue graphicsQueue,
                   VkQueue presentaionQueue, VkSwapchainKHR swapchain)
      : _device(device), GraphicsQueue(graphicsQueue),
        PresentationQueue(presentaionQueue), Swapchain(swapchain) {}

public:
  ~SwapchainManager();
  static std::shared_ptr<SwapchainManager>
  create(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkDevice device,
         uint32_t graphicsQueueIndex, VkQueue graphicsQueue,
         VkQueue presentaionQueue, VkRenderPass renderPass,
         VkSwapchainKHR oldSwapchain);
  VkSwapchainKHR Handle() const { return Swapchain; }
  VkExtent2D size() const { return _size; }

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

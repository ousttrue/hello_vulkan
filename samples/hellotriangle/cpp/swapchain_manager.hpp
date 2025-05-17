#pragma once
#include "backbuffer.hpp"

#include <vulkan/vulkan.h>

#include <vector>

class SwapchainManager {
  VkDevice _device;
  VkQueue _graphicsQueue;
  VkQueue _presentationQueue;
  VkSwapchainKHR _swapchain;
  VkExtent2D _size;
  VkFormat _format;

  std::vector<VkImage> _swapchainImages;
  std::vector<std::shared_ptr<class Backbuffer>> _backbuffers;
  unsigned _swapchainIndex = 0;
  unsigned _renderingThreadCount = 0;

  SwapchainManager(VkDevice device, VkQueue graphicsQueue,
                   VkQueue presentaionQueue, VkSwapchainKHR swapchain)
      : _device(device), _graphicsQueue(graphicsQueue),
        _presentationQueue(presentaionQueue), _swapchain(swapchain) {}

public:
  ~SwapchainManager();
  static std::shared_ptr<SwapchainManager>
  create(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkDevice device,
         uint32_t graphicsQueueIndex, VkQueue graphicsQueue,
         VkQueue presentaionQueue, VkRenderPass renderPass,
         VkSwapchainKHR oldSwapchain);
  VkSwapchainKHR Handle() const { return _swapchain; }
  VkExtent2D size() const { return _size; }

  std::shared_ptr<Backbuffer> getBackbuffer(uint32_t index) const {
    return _backbuffers[index];
  }

private:
  void submitCommandBuffer(VkCommandBuffer, VkSemaphore acquireSemaphore,
                           VkSemaphore releaseSemaphore);
  void setRenderingThreadCount(unsigned count);

public:
  VkSemaphore beginFrame(unsigned index, VkSemaphore acquireSemaphore) {
    _swapchainIndex = index;
    _backbuffers[_swapchainIndex]->beginFrame();
    return _backbuffers[_swapchainIndex]->setSwapchainAcquireSemaphore(
        acquireSemaphore);
  }
  VkCommandBuffer beginRender(const std::shared_ptr<Backbuffer> &backbuffer);
  void submitSwapchain(VkCommandBuffer cmdBuffer);
  bool presentImage(unsigned index);
};

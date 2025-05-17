#pragma once
#include "backbuffer.hpp"

#include <vulkan/vulkan.h>

#include <vector>
#include <vulkan/vulkan_core.h>

class SwapchainManager {
  VkDevice _device;
  VkQueue _presentationQueue;
  VkSwapchainKHR _swapchain;
  VkExtent2D _size;
  VkFormat _format;

  std::vector<VkImage> _swapchainImages;
  std::vector<std::shared_ptr<class Backbuffer>> _backbuffers;
  unsigned _swapchainIndex = 0;
  std::shared_ptr<class SemaphoreManager> _semaphoreManager;

  SwapchainManager(VkDevice device, VkQueue presentaionQueue,
                   VkSwapchainKHR swapchain);

public:
  ~SwapchainManager();
  static std::shared_ptr<SwapchainManager>
  create(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkDevice device,
         uint32_t graphicsQueueIndex, VkQueue presentaionQueue,
         VkRenderPass renderPass, VkSwapchainKHR oldSwapchain);
  VkSwapchainKHR handle() const { return _swapchain; }
  std::tuple<VkResult, VkSemaphore, std::shared_ptr<Backbuffer>> AcquireNext();
  void sync(VkQueue queue, VkSemaphore acquireSemaphore);
  void addClearedSemaphore(VkSemaphore semaphore);
  VkExtent2D size() const { return _size; }
};

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

  SwapchainManager(VkDevice device, uint32_t presentFamily,
                   VkSwapchainKHR swapchain);

public:
  ~SwapchainManager();
  static std::shared_ptr<SwapchainManager>
  create(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkDevice device,
         uint32_t graphicsFamily, uint32_t presentaionFamily,
         VkRenderPass renderPass, VkSwapchainKHR oldSwapchain);
  VkSwapchainKHR handle() const { return _swapchain; }
  std::tuple<VkResult, std::shared_ptr<Backbuffer>>
  AcquireNext(VkSemaphore acquireSemaphore);
  VkExtent2D size() const { return _size; }
};

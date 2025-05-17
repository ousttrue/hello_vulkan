#pragma once
#include "command_buffer_manager.hpp"
#include "fence_manager.hpp"

#include <vulkan/vulkan.h>

struct Backbuffer {
  uint32_t _index;
  VkDevice _device;
  uint32_t _graphicsQueueIndex;

  // VkImage _image;
  VkImageView _view;
  VkFramebuffer _framebuffer;
  FenceManager _fenceManager;
  CommandBufferManager _commandManager;
  std::vector<std::unique_ptr<CommandBufferManager>> _secondaryCommandManagers;
  VkSemaphore _swapchainAcquireSemaphore = VK_NULL_HANDLE;
  VkSemaphore _swapchainReleaseSemaphore = VK_NULL_HANDLE;

public:
  Backbuffer(uint32_t i, VkDevice device, uint32_t graphicsQueueIndex,
             VkImage image, VkFormat format, VkExtent2D size,
             VkRenderPass renderPass);
  ~Backbuffer();

  void beginFrame();
  VkSemaphore setSwapchainAcquireSemaphore(VkSemaphore acquireSemaphore);
  void setSwapchainReleaseSemaphore(VkSemaphore releaseSemaphore);
  void setSecondaryCommandManagersCount(unsigned count);
};

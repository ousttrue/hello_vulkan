#pragma once
#include "command_buffer_manager.hpp"
#include "fence_manager.hpp"

#include <vulkan/vulkan.h>

class Backbuffer {
  uint32_t _index;
  VkDevice _device;
  VkImageView _view;
  VkFramebuffer _framebuffer;
  FenceManager _fenceManager;
  CommandBufferManager _commandManager;

  VkSemaphore _swapchainAcquireSemaphore = VK_NULL_HANDLE;
  VkSemaphore _swapchainReleaseSemaphore = VK_NULL_HANDLE;

public:
  Backbuffer(uint32_t i, VkDevice device, uint32_t graphicsQueueIndex,
             VkImage image, VkFormat format, VkExtent2D size,
             VkRenderPass renderPass);
  ~Backbuffer();

  VkFramebuffer framebuffer() const { return _framebuffer; }
  std::tuple<VkSemaphore, VkCommandBuffer>
  beginFrame(VkSemaphore acquireSemaphore);
  VkResult endFrame(VkQueue graphicsQueue, VkCommandBuffer cmd,
                    VkQueue presentationQueue, VkSwapchainKHR swapchain);

private:
  void setSwapchainReleaseSemaphore(VkSemaphore releaseSemaphore);
};

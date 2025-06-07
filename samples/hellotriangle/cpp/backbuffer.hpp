#pragma once
#include <vulkan/vulkan.h>

class Backbuffer {
  uint32_t _index;
  VkDevice _device;
  VkImageView _view;
  VkFramebuffer _framebuffer;

  VkSemaphore _swapchainAcquireSemaphore = VK_NULL_HANDLE;
  VkSemaphore _swapchainReleaseSemaphore = VK_NULL_HANDLE;

public:
  Backbuffer(uint32_t i, VkDevice device, VkImage image, VkFormat format,
             VkExtent2D size, VkRenderPass renderPass);
  ~Backbuffer();

  VkFramebuffer framebuffer() const { return _framebuffer; }
  VkSemaphore beginFrame(VkCommandBuffer cmd, VkSemaphore acquireSemaphore);
  VkResult submit(VkQueue graphicsQueue, VkCommandBuffer cmd, VkFence fence,
                  VkQueue presentationQueue, VkSwapchainKHR swapchain);
};

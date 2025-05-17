#pragma once
#include <vulkan/vulkan.h>

#include <vector>

class CommandBufferManager {
  VkDevice _device = VK_NULL_HANDLE;
  VkCommandBufferLevel _commandBufferLevel;
  unsigned _count = 0;

  VkCommandPool _pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> _buffers;

public:
  CommandBufferManager(VkDevice device, VkCommandBufferLevel bufferLevel,
                       unsigned graphicsQueueIndex);
  ~CommandBufferManager();
  VkCommandBuffer requestCommandBuffer();
  void beginFrame();
};

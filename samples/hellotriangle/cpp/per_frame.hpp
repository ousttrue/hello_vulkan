#pragma once
#include "command_buffer_manager.hpp"
#include "fence_manager.hpp"

namespace MaliSDK {

struct PerFrame {
  PerFrame(VkDevice device, unsigned graphicsQueueIndex);
  ~PerFrame();

  void beginFrame();
  VkSemaphore setSwapchainAcquireSemaphore(VkSemaphore acquireSemaphore);
  void setSwapchainReleaseSemaphore(VkSemaphore releaseSemaphore);
  void setSecondaryCommandManagersCount(unsigned count);

  VkDevice device = VK_NULL_HANDLE;
  FenceManager fenceManager;
  CommandBufferManager commandManager;
  std::vector<std::unique_ptr<CommandBufferManager>> secondaryCommandManagers;
  VkSemaphore swapchainAcquireSemaphore = VK_NULL_HANDLE;
  VkSemaphore swapchainReleaseSemaphore = VK_NULL_HANDLE;
  unsigned queueIndex;
};

} // namespace MaliSDK

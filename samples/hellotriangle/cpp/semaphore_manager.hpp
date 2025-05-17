#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class SemaphoreManager {
  VkDevice device = VK_NULL_HANDLE;
  std::vector<VkSemaphore> recycledSemaphores;

public:
  SemaphoreManager(VkDevice device);
  ~SemaphoreManager();
  VkSemaphore getClearedSemaphore();
  void addClearedSemaphore(VkSemaphore semaphore);
};

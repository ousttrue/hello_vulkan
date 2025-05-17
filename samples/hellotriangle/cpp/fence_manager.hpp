#pragma once
#include <vulkan/vulkan.h>

#include <vector>

class FenceManager {
  VkDevice _device = VK_NULL_HANDLE;
  std::vector<VkFence> _fences;
  unsigned _count = 0;

public:
  FenceManager(VkDevice device);
  ~FenceManager();
  void beginFrame();
  VkFence requestClearedFence();
  // unsigned getActiveFenceCount() const { return _count; }
  // VkFence *getActiveFences() { return _fences.data(); }
};

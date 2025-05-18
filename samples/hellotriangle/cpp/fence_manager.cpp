#include "fence_manager.hpp"
#include "logger.hpp"

FenceManager::FenceManager(VkDevice vkDevice) : _device(vkDevice), _count(0) {}

FenceManager::~FenceManager() {
  beginFrame();
  for (auto &fence : _fences) {
    vkDestroyFence(_device, fence, nullptr);
  }
}

void FenceManager::beginFrame() {
  if (_count == 0) {
    return;
  }
  // If we have outstanding fences for this swapchain image, wait for them to
  // complete first.
  // Normally, this doesn't really block at all,
  // since we're waiting for old frames to have been completed, but just in
  // case.
  vkWaitForFences(_device, _count, _fences.data(), true, UINT64_MAX);
  vkResetFences(_device, _count, _fences.data());
  _count = 0;
}

VkFence FenceManager::requestClearedFence() {
  if (_count >= _fences.size()) {
    VkFence fence;
    VkFenceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if (vkCreateFence(_device, &info, nullptr, &fence) != VK_SUCCESS) {
      LOGE("vkCreateFence");
      abort();
    };
    _fences.push_back(fence);
  }
  return _fences[_count++];
}

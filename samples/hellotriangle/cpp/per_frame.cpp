#include "per_frame.hpp"

namespace MaliSDK {

PerFrame::PerFrame(VkDevice device, unsigned graphicsQueueIndex)
    : device(device), fenceManager(device),
      commandManager(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                     graphicsQueueIndex),
      queueIndex(graphicsQueueIndex) {}

PerFrame::~PerFrame() {
  if (swapchainAcquireSemaphore != VK_NULL_HANDLE)
    vkDestroySemaphore(device, swapchainAcquireSemaphore, nullptr);
  if (swapchainReleaseSemaphore != VK_NULL_HANDLE)
    vkDestroySemaphore(device, swapchainReleaseSemaphore, nullptr);
}

void PerFrame::setSecondaryCommandManagersCount(unsigned count) {
  secondaryCommandManagers.clear();
  for (unsigned i = 0; i < count; i++) {
    secondaryCommandManagers.emplace_back(new CommandBufferManager(
        device, VK_COMMAND_BUFFER_LEVEL_SECONDARY, queueIndex));
  }
}

VkSemaphore
PerFrame::setSwapchainAcquireSemaphore(VkSemaphore acquireSemaphore) {
  VkSemaphore ret = swapchainAcquireSemaphore;
  swapchainAcquireSemaphore = acquireSemaphore;
  return ret;
}

void PerFrame::setSwapchainReleaseSemaphore(VkSemaphore releaseSemaphore) {
  if (swapchainReleaseSemaphore != VK_NULL_HANDLE)
    vkDestroySemaphore(device, swapchainReleaseSemaphore, nullptr);
  swapchainReleaseSemaphore = releaseSemaphore;
}

void PerFrame::beginFrame() {
  fenceManager.beginFrame();
  commandManager.beginFrame();
  for (auto &pManager : secondaryCommandManagers)
    pManager->beginFrame();
}

} // namespace MaliSDK

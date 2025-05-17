#include "semaphore_manager.hpp"
#include "common.hpp"

SemaphoreManager::SemaphoreManager(VkDevice vkDevice) : device(vkDevice) {}

SemaphoreManager::~SemaphoreManager() {
  for (auto &semaphore : recycledSemaphores)
    vkDestroySemaphore(device, semaphore, nullptr);
}

void SemaphoreManager::addClearedSemaphore(VkSemaphore semaphore) {
  recycledSemaphores.push_back(semaphore);
}

VkSemaphore SemaphoreManager::getClearedSemaphore() {
  if (recycledSemaphores.empty()) {
    VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
  } else {
    auto semaphore = recycledSemaphores.back();
    recycledSemaphores.pop_back();
    return semaphore;
  }
}

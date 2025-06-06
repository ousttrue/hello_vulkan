#pragma once
#include <memory>
#include <vulkan/vulkan.h>

class MemoryAllocator {
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkPhysicalDeviceMemoryProperties m_memProps{};

public:
  static std::shared_ptr<MemoryAllocator>
  Create(VkPhysicalDevice physicalDevice, VkDevice device);
  static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  void Allocate(VkMemoryRequirements const &memReqs, VkDeviceMemory *mem,
                VkFlags flags = defaultFlags,
                const void *pNext = nullptr) const;
};

#include "MemoryAllocator.h"
#include <vulkan/vulkan_core.h>

#include <stdexcept>

std::shared_ptr<MemoryAllocator>
MemoryAllocator::Create(VkPhysicalDevice physicalDevice, VkDevice device) {
  auto ptr = std::shared_ptr<MemoryAllocator>(new MemoryAllocator);
  ptr->m_vkDevice = device;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &ptr->m_memProps);
  return ptr;
}

void MemoryAllocator::Allocate(VkMemoryRequirements const &memReqs,
                               VkDeviceMemory *mem, VkFlags flags,
                               const void *pNext) const {
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
    if ((memReqs.memoryTypeBits & (1 << i)) != 0u) {
      // Type is available, does it match user properties?
      if ((m_memProps.memoryTypes[i].propertyFlags & flags) == flags) {
        VkMemoryAllocateInfo memAlloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = pNext,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = i,
        };
        if (vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, mem) !=
            VK_SUCCESS) {
          throw std::runtime_error("vkAllocateMemory");
        }
        return;
      }
    }
  }
  throw std::runtime_error("Memory format not supported");
}

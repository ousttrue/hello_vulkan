#pragma once
#include "../vk.h"
#include "vuloxr.h"
#include <span>

namespace vuloxr {

namespace vk {

inline VkBuffer createVertexBuffer(VkDevice device, uint32_t bufferSize) {
  VkBufferCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
  };
  VkBuffer buffer;
  vuloxr::vk::CheckVkResult(vkCreateBuffer(device, &info, nullptr, &buffer));
  return buffer;
}

inline uint32_t
findMemoryTypeFromRequirements(const VkPhysicalDeviceMemoryProperties &props,
                               uint32_t deviceRequirements,
                               uint32_t hostRequirements) {
  for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
    if (deviceRequirements & (1u << i)) {
      if ((props.memoryTypes[i].propertyFlags & hostRequirements) ==
          hostRequirements) {
        return i;
      }
    }
  }

  vuloxr::Logger::Error("Failed to obtain suitable memory type.\n");
  abort();
}

inline VkDeviceMemory createBindMemory(VkPhysicalDevice physicalDevice,
                                       VkDevice device, VkBuffer buffer,
                                       const void *src, uint32_t size) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(device, buffer, &memReqs);
  VkMemoryAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // We want host visible and coherent memory to simplify things.
      .memoryTypeIndex = findMemoryTypeFromRequirements(
          props, memReqs.memoryTypeBits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };
  VkDeviceMemory memory;
  vuloxr::vk::CheckVkResult(vkAllocateMemory(device, &alloc, nullptr, &memory));
  vkBindBufferMemory(device, buffer, memory, 0);
  {
    void *dst;
    CheckVkResult(vkMapMemory(device, memory, 0, size, 0, &dst));
    memcpy(dst, src, size);
    vkUnmapMemory(device, memory);
  }
  return memory;
}

struct Mesh : NonCopyable {
  VkDevice device;
  VkBuffer vertices;
  uint32_t vertexCount;
  VkDeviceMemory verticesMemory;
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  ~Mesh() {
    vkFreeMemory(this->device, this->verticesMemory, nullptr);
    vkDestroyBuffer(this->device, this->vertices, nullptr);
  }

  template <typename T>
  static Mesh
  create(VkPhysicalDevice physicalDevice, VkDevice device,
         std::span<const T> vertices,
         const std::vector<VkVertexInputBindingDescription> &bindings,
         const std::vector<VkVertexInputAttributeDescription> &attributes) {
    auto bufferSize = sizeof(T) * vertices.size();
    auto buffer = createVertexBuffer(device, bufferSize);
    auto memory = createBindMemory(physicalDevice, device, buffer,
                                   vertices.data(), bufferSize);
    return {
        .device = device,
        .vertices = buffer,
        .vertexCount = static_cast<uint32_t>(vertices.size()),
        .verticesMemory = memory,
        .bindings = bindings,
        .attributes = attributes,
    };
  }

  void draw(VkCommandBuffer cmd, VkPipeline pipeline) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &this->vertices, &offset);
    vkCmdDraw(cmd, this->vertexCount, 1, 0, 0);
  }
};

} // namespace vk
} // namespace vuloxr

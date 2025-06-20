#pragma once
#include "../vk.h"
#include "vuloxr.h"
#include <span>

namespace vuloxr {

namespace vk {

struct Mesh : NonCopyable {
  VkDevice device;
  VkBuffer buffer;
  uint32_t vertexCount;
  VkDeviceMemory memory;
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  Mesh(VkDevice _device, VkBuffer _buffer, VkDeviceMemory _memory)
      : device(_device), buffer(_buffer), memory(_memory) {}

  ~Mesh() {
    vkFreeMemory(this->device, this->memory, nullptr);
    vkDestroyBuffer(this->device, this->buffer, nullptr);
  }

  static Mesh create(VkPhysicalDevice physicalDevice, VkDevice device,
                     const void *vertices, uint32_t bufferSize) {
    // uint32_t bufferSize = sizeof(T) * vertices.size();
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    VkBuffer buffer;
    vuloxr::vk::CheckVkResult(vkCreateBuffer(device, &info, nullptr, &buffer));

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
    vuloxr::vk::CheckVkResult(
        vkAllocateMemory(device, &alloc, nullptr, &memory));

    // Buffers are not backed by memory, so bind our memory explicitly to the
    // buffer.
    vkBindBufferMemory(device, buffer, memory, 0);

    // Map the memory and dump data in there.
    {
      void *pData;
      CheckVkResult(vkMapMemory(device, memory, 0, bufferSize, 0, &pData));
      memcpy(pData, vertices, bufferSize);
      vkUnmapMemory(device, memory);
    }

    return Mesh(device, buffer, memory);
  }

  static uint32_t
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

  void draw(VkCommandBuffer cmd, VkPipeline pipeline) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &this->buffer, &offset);

    vkCmdDraw(cmd, this->vertexCount, 1, 0, 0);
  }
};

} // namespace vk

} // namespace vuloxr

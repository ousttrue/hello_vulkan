#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct DeviceMemory {
  VkDevice device;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  DeviceMemory(VkDevice _device, VkPhysicalDevice physicalDevice,
               VkBuffer buffer, VkMemoryPropertyFlags properties)
      : device(_device) {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);
    VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(
            physicalDevice, memRequirements.memoryTypeBits, properties),
    };
    if (vkAllocateMemory(this->device, &allocateInfo, nullptr, &this->memory) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
  }
  ~DeviceMemory() {
    if (this->memory != VK_NULL_HANDLE) {
      vkFreeMemory(this->device, this->memory, nullptr);
    }
  }
  static uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
              properties) {
        return i;
      }
    }
    throw std::runtime_error("failed to find suitable memory type!");
  }
};

class BufferObject {
  VkDevice _device;
  VkBuffer _buffer;
  std::shared_ptr<DeviceMemory> _memory;

public:
  BufferObject(VkPhysicalDevice physicalDevice, VkDevice device,
               VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties);
  ~BufferObject();
  VkBuffer buffer() const { return _buffer; }
  void copy(const void *src, size_t size);
  template <typename T> void copy(const T &src) { copy(&src, sizeof(src)); }
  void copyTo(VkCommandBuffer commandBuffer, VkBuffer dstBuffer,
              VkDeviceSize size);
};

class TextureObject {
  VkDevice _device;
  VkImage _image;
  VkDeviceMemory _imageMemory;

public:
  TextureObject(VkDevice device, VkImage image, VkDeviceMemory imageMemory);
  ~TextureObject();
  VkImage image() const { return _image; }
};

class MemoryAllocator {
  VkPhysicalDeviceMemoryProperties _memProperties;
  VkDevice _device;
  VkQueue _graphicsQueue;
  VkCommandPool _commandPool;

public:
  MemoryAllocator(VkPhysicalDevice physicalDevice, VkDevice device,
                  uint32_t graphicsQueueFamilyIndex);
  ~MemoryAllocator();

  // Find the correct type of memory to use
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  std::shared_ptr<class TextureObject>
  createImage(uint32_t width, uint32_t height, VkFormat format,
              VkImageTiling tiling, VkImageUsageFlags usage,
              VkMemoryPropertyFlags properties);

  void oneTimeCommandSync(
      const std::function<void(VkCommandBuffer)> &commandRecorder);
};

#pragma once
#include <functional>
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class BufferObject {
  VkDevice _device;
  VkBuffer _buffer;
  VkDeviceMemory _bufferMemory;

public:
  BufferObject(VkDevice device, VkBuffer buffer, VkDeviceMemory bufferMemory);
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
  MemoryAllocator(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamilyIndex);
  ~MemoryAllocator();

  // Find the correct type of memory to use
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  std::shared_ptr<class BufferObject>
  createBuffer(const void *p, VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties);

  std::shared_ptr<class TextureObject>
  createImage(uint32_t width, uint32_t height, VkFormat format,
              VkImageTiling tiling, VkImageUsageFlags usage,
              VkMemoryPropertyFlags properties);

  void oneTimeCommandSync(
      const std::function<void(VkCommandBuffer)> &commandRecorder);
};

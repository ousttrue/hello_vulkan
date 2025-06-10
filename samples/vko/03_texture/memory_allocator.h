#pragma once
#include <functional>
#include <memory>
#include <vko/vko_pipeline.h>

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

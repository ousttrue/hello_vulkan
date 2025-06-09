#include "memory_allocator.h"
#include <stdexcept>

BufferObject::BufferObject(VkDevice device, VkBuffer buffer,
                           VkDeviceMemory bufferMemory)
    : _device(device), _buffer(buffer), _bufferMemory(bufferMemory) {
  vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

BufferObject::~BufferObject() {
  vkDestroyBuffer(_device, _buffer, nullptr);
  vkFreeMemory(_device, _bufferMemory, nullptr);
}

void BufferObject::copy(const void *p, size_t size) {
  void *data;
  vkMapMemory(_device, _bufferMemory, 0, size, 0, &data);
  memcpy(data, p, size);
  vkUnmapMemory(_device, _bufferMemory);
}

void BufferObject::copyTo(VkCommandBuffer commandBuffer, VkBuffer dstBuffer,
                          VkDeviceSize size) {
  VkBufferCopy copyRegion{
      .srcOffset = 0, // optional
      .dstOffset = 0, // optional
      .size = size,
  };
  vkCmdCopyBuffer(commandBuffer, _buffer, dstBuffer, 1, &copyRegion);
}

TextureObject::TextureObject(VkDevice device, VkImage image,
                             VkDeviceMemory imageMemory)
    : _device(device), _image(image), _imageMemory(imageMemory) {
  vkBindImageMemory(device, image, imageMemory, 0);
}

TextureObject::~TextureObject() {
  vkDestroyImage(_device, _image, nullptr);
  vkFreeMemory(_device, _imageMemory, nullptr);
}

MemoryAllocator::MemoryAllocator(VkPhysicalDevice physicalDevice,
                                 VkDevice device,
                                 uint32_t graphicsQueueFamilyIndex)
    : _device(device) {
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &_memProperties);

  vkGetDeviceQueue(_device, graphicsQueueFamilyIndex, 0, &_graphicsQueue);
  VkCommandPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = 0,
      .queueFamilyIndex = graphicsQueueFamilyIndex,
  };
  if (vkCreateCommandPool(device, &poolInfo, nullptr, &_commandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
  }
}

MemoryAllocator::~MemoryAllocator(){
  vkDestroyCommandPool(_device, _commandPool, nullptr);
}

std::shared_ptr<BufferObject>
// Helper function for buffer creation
MemoryAllocator::createBuffer(const void *p, VkDeviceSize size,
                              VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags properties) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkBuffer buffer;
  if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      findMemoryType(memRequirements.memoryTypeBits, properties);

  VkDeviceMemory bufferMemory;
  if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate vertex buffer memory!");
  }

  if (p) {
    void *data;
    vkMapMemory(_device, bufferMemory, 0, size, 0, &data);
    memcpy(data, p, static_cast<size_t>(size));
    vkUnmapMemory(_device, bufferMemory);
  }

  auto ptr = std::shared_ptr<BufferObject>(
      new BufferObject(_device, buffer, bufferMemory));
  return ptr;
}

// Find the correct type of memory to use
uint32_t MemoryAllocator::findMemoryType(uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) {

  for (uint32_t i = 0; i < _memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) &&
        (_memProperties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
}

std::shared_ptr<TextureObject>
MemoryAllocator::createImage(uint32_t width, uint32_t height, VkFormat format,
                             VkImageTiling tiling, VkImageUsageFlags usage,
                             VkMemoryPropertyFlags properties) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format =
      format; // This needs to match the format of the pixels in the buffer
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.flags = VK_SHARING_MODE_EXCLUSIVE;

  VkImage image;
  if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    throw std::runtime_error("failed to createimage!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(_device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      findMemoryType(memRequirements.memoryTypeBits, properties);

  VkDeviceMemory imageMemory;
  if (vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }

  auto ptr = std::shared_ptr<TextureObject>(
      new TextureObject(_device, image, imageMemory));
  return ptr;
}

static VkCommandBuffer beginSingleTimeCommands(VkDevice device,
                                               VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool,
                           VkQueue graphicsQueue,
                           VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };
  vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void MemoryAllocator::oneTimeCommandSync(
    const std::function<void(VkCommandBuffer)> &commandRecorder) {
  auto command = beginSingleTimeCommands(_device, _commandPool);
  commandRecorder(command);
  endSingleTimeCommands(_device, _commandPool, _graphicsQueue, command);
}

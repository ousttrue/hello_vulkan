#pragma once
#include <memory>
#include <vulkan/vulkan.h>

class DepthBuffer {
  VkDeviceMemory depthMemory{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  DepthBuffer() {}

public:
  VkImage depthImage{VK_NULL_HANDLE};
  ~DepthBuffer();

  // DepthBuffer(DepthBuffer &&other) noexcept;
  // DepthBuffer &operator=(DepthBuffer &&other) noexcept;
  static std::shared_ptr<DepthBuffer>
  Create(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D size,
         VkFormat depthFormat, VkSampleCountFlagBits sampleCount);

  void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout);
  DepthBuffer(const DepthBuffer &) = delete;
  DepthBuffer &operator=(const DepthBuffer &) = delete;
};

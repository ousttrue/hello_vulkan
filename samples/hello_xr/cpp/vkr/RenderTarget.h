#pragma once
#include <memory>
#include <vulkan/vulkan.h>

// VkImage + framebuffer wrapper
class RenderTarget {
  VkImage colorImage{VK_NULL_HANDLE};
  VkImage depthImage{VK_NULL_HANDLE};
  VkImageView colorView{VK_NULL_HANDLE};
  VkImageView depthView{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};

  RenderTarget() = default;

public:
  VkFramebuffer fb{VK_NULL_HANDLE};
  ~RenderTarget();

  RenderTarget(RenderTarget &&other) noexcept;
  RenderTarget &operator=(RenderTarget &&other) noexcept;
  static std::shared_ptr<RenderTarget>
  Create(VkDevice device, VkImage aColorImage, VkImage aDepthImage,
         VkExtent2D size, const std::shared_ptr<class RenderPass> &renderPass);

  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;
};
